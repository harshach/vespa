// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/vdstestlib/cppunit/macros.h>
#include <vespa/vespalib/objects/floatingpointtype.h>
#include <vespa/metrics/metrics.h>

namespace metrics {

struct MetricSetTest : public CppUnit::TestFixture {
    void testNormalUsage();
    void supportMultipleMetricsWithSameNameDifferentDimensions();
    void uniqueTargetMetricsAreAddedToMetricSet();

    CPPUNIT_TEST_SUITE(MetricSetTest);
    CPPUNIT_TEST(testNormalUsage);
    CPPUNIT_TEST(supportMultipleMetricsWithSameNameDifferentDimensions);
    CPPUNIT_TEST(uniqueTargetMetricsAreAddedToMetricSet);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(MetricSetTest);

namespace {
    struct TestMetricVisitor : public MetricVisitor {
        std::ostringstream ost;
        uint32_t setsToVisit;

        TestMetricVisitor(uint32_t setsToVisit_ = 100)
            : ost(), setsToVisit(setsToVisit_) {}

        bool visitMetricSet(const MetricSet& set, bool autoGenerated) override {
            ost << "[" << (autoGenerated ? "*" : "") << set.getName() << "]\n";
            if (setsToVisit > 0) {
                --setsToVisit;
                return true;
            }
            return false;
        }
        bool visitMetric(const Metric& m, bool autoGenerated) override {
            ost << (autoGenerated ? "*" : "") << m.getName() << "\n";
            return true;
        }
    };
}

void
MetricSetTest::testNormalUsage()
{
        // Set up some metrics to test..
    MetricSet set("a", "foo", "");
    DoubleValueMetric v1("c", "foo", "", &set);
    LongAverageMetric v2("b", "", "", &set);
    LongCountMetric v3("d", "bar", "", &set);
    MetricSet set2("e", "bar", "", &set);
    LongCountMetric v4("f", "foo", "", &set2);

        // Give them some values
    v1.addValue(4.2);
    v2.addValue(8);
    v3.inc();
    v4.inc(3);

        // Check that we can register through registerMetric function too.
    LongCountMetric v5("g", "", "");
    set.registerMetric(v5);
    v5.inc(3);
    v5.dec();

        // Check that getMetric works, and doesn't return copy.
    LongAverageMetric* v2copy(
            dynamic_cast<LongAverageMetric*>(set.getMetric("b")));
    CPPUNIT_ASSERT(v2copy != 0);
    v2copy->addValue(9);
    Metric* nonExistingCopy = set.getMetric("nonexisting");
    CPPUNIT_ASSERT(nonExistingCopy == 0);
    nonExistingCopy = set.getMetric("non.existing");
    CPPUNIT_ASSERT(nonExistingCopy == 0);

        // Check that paths are set
    MetricSet topSet("top", "", "");
    topSet.registerMetric(set);
    CPPUNIT_ASSERT_EQUAL(vespalib::string("a"), set.getPath());
    CPPUNIT_ASSERT_EQUAL(vespalib::string("a.c"), v1.getPath());
    CPPUNIT_ASSERT_EQUAL(vespalib::string("a.b"), v2.getPath());
    CPPUNIT_ASSERT_EQUAL(vespalib::string("a.d"), v3.getPath());
    CPPUNIT_ASSERT_EQUAL(vespalib::string("a.e"), set2.getPath());
    CPPUNIT_ASSERT_EQUAL(vespalib::string("a.e.f"), v4.getPath());
    CPPUNIT_ASSERT_EQUAL(vespalib::string("a.g"), v5.getPath());
    CPPUNIT_ASSERT_EQUAL(vespalib::string("a.b"), v2copy->getPath());

        // Verify XML output. Should be in register order.
    std::string expected("'\n"
            "a:\n"
            "  c average=4.2 last=4.2 min=4.2 max=4.2 count=1 total=4.2\n"
            "  b average=8.5 last=9 min=8 max=9 count=2 total=17\n"
            "  d count=1\n"
            "  e:\n"
            "    f count=3\n"
            "  g count=2'"
    );
    CPPUNIT_ASSERT_EQUAL(expected, "'\n" + set.toString() + "'");

        // Verify that visiting works. That you get all metrics if you answer
        // true to all sets, and that you don't get members of sets you answer
        // false to get.
    {
        TestMetricVisitor visitor(2);
        set.visit(visitor);
        expected = "[a]\nc\nb\nd\n[e]\nf\ng\n";
        CPPUNIT_ASSERT_EQUAL("\n" + expected, "\n" + visitor.ost.str());
    }
    {
        TestMetricVisitor visitor(1);
        set.visit(visitor);
        expected = "[a]\nc\nb\nd\n[e]\ng\n";
        CPPUNIT_ASSERT_EQUAL("\n" + expected, "\n" + visitor.ost.str());
    }
    {
        TestMetricVisitor visitor(0);
        set.visit(visitor);
        expected = "[a]\n";
        CPPUNIT_ASSERT_EQUAL("\n" + expected, "\n" + visitor.ost.str());
    }
}

void
MetricSetTest::supportMultipleMetricsWithSameNameDifferentDimensions()
{
    MetricSet set("dimset", {{"foo", "bar"}}, "");
    DoubleValueMetric v1("stuff", {{"baz", "blarg"}}, "", &set);
    LongAverageMetric v2("stuff", {{"flarn", "yarn"}}, "", &set);

    CPPUNIT_ASSERT_EQUAL(size_t(2), set.getRegisteredMetrics().size());
}

void
MetricSetTest::uniqueTargetMetricsAreAddedToMetricSet()
{
    MetricSet set1("a", "foo", "");
    LongCountMetric v1("wow", "foo", "", &set1);
    MetricSet set2("e", "bar", "");
    LongCountMetric v2("doge", "foo", "", &set2);

    // Have to actually assign a value to metrics or they won't be carried over.
    v1.inc();
    v2.inc();

    // 'doge' metric in set2 must be preserved even though it does not exist
    // in set1.
    std::vector<Metric::UP> ownerList;
    set1.addToSnapshot(set2, ownerList);

    CPPUNIT_ASSERT(set2.getMetric("wow") != nullptr);
    CPPUNIT_ASSERT(set2.getMetric("doge") != nullptr);
}

} // metrics
