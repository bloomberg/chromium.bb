// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/metric.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace performance_monitor {

// A clock that increments every access. Great for testing.
class TestingClock : public Database::Clock {
 public:
  TestingClock()
      : counter_(0) {
  }
  explicit TestingClock(const TestingClock& other)
      : counter_(other.counter_) {
  }
  virtual ~TestingClock() {}
  base::Time GetTime() {
    return base::Time::FromInternalValue(++counter_);
  }
 private:
  int64 counter_;
};

class PerformanceMonitorDatabaseEventTest : public ::testing::Test {
 protected:
  PerformanceMonitorDatabaseEventTest() {
    clock_ = new TestingClock();
    CHECK(temp_dir_.CreateUniqueTempDir());
    db_ = Database::Create(temp_dir_.path());
    CHECK(db_.get());
    db_->set_clock(scoped_ptr<Database::Clock>(clock_));
  }

  void SetUp() {
    ASSERT_TRUE(db_.get());
    PopulateDB();
  }

  void PopulateDB() {
    InitEvents();
    db_->AddEvent(*install_event_1_.get());
    db_->AddEvent(*install_event_2_.get());
    db_->AddEvent(*uninstall_event_1_.get());
    db_->AddEvent(*uninstall_event_2_.get());
  }

  scoped_ptr<Database> db_;
  Database::Clock* clock_;
  ScopedTempDir temp_dir_;
  scoped_ptr<Event> install_event_1_;
  scoped_ptr<Event> install_event_2_;
  scoped_ptr<Event> uninstall_event_1_;
  scoped_ptr<Event> uninstall_event_2_;

 private:
  void InitEvents() {
    install_event_1_ = util::CreateExtensionEvent(
        EVENT_EXTENSION_INSTALL, clock_->GetTime(), "a", "extension 1",
        "http://foo.com", static_cast<int>(Extension::LOAD), "0.1",
        "Test Test");
    install_event_2_ = util::CreateExtensionEvent(
        EVENT_EXTENSION_INSTALL, clock_->GetTime(), "b", "extension 2",
        "http://bar.com", static_cast<int>(Extension::LOAD), "0.1",
        "Test Test");
    uninstall_event_1_ = util::CreateExtensionEvent(
        EVENT_EXTENSION_UNINSTALL, clock_->GetTime(), "a", "extension 1",
        "http://foo.com", static_cast<int>(Extension::LOAD), "0.1",
        "Test Test");
    uninstall_event_2_ = util::CreateExtensionEvent(
        EVENT_EXTENSION_UNINSTALL, clock_->GetTime(), "b", "extension 2",
        "http://bar.com", static_cast<int>(Extension::LOAD), "0.1",
        "Test Test");
  }
};

class PerformanceMonitorDatabaseMetricTest : public ::testing::Test {
 protected:
  PerformanceMonitorDatabaseMetricTest() {
    clock_ = new TestingClock();
    CHECK(temp_dir_.CreateUniqueTempDir());
    db_ = Database::Create(temp_dir_.path());
    CHECK(db_.get());
    db_->set_clock(scoped_ptr<Database::Clock>(clock_));
    activity_ = std::string("A");
  }

  void SetUp() {
    ASSERT_TRUE(db_.get());
    PopulateDB();
  }

  void PopulateDB() {
    db_->AddMetric(kProcessChromeAggregate, METRIC_CPU_USAGE,
                   std::string("50.5"));
    db_->AddMetric(activity_, METRIC_CPU_USAGE, std::string("13.1"));

    db_->AddMetric(kProcessChromeAggregate, METRIC_PRIVATE_MEMORY_USAGE,
                   std::string("1000000"));
    db_->AddMetric(activity_, METRIC_PRIVATE_MEMORY_USAGE,
                   std::string("3000000"));
  }

  scoped_ptr<Database> db_;
  Database::Clock* clock_;
  ScopedTempDir temp_dir_;
  std::string activity_;
};

////// PerformanceMonitorDatabaseSetupTests ////////////////////////////////////
TEST(PerformanceMonitorDatabaseSetupTest, OpenClose) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_ptr<Database> db = Database::Create(temp_dir.path());
  ASSERT_TRUE(db.get());
  ASSERT_TRUE(db->Close());
}

TEST(PerformanceMonitorDatabaseSetupTest, ActiveInterval) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  TestingClock* clock_1 = new TestingClock();
  base::Time start_time = clock_1->GetTime();

  scoped_ptr<Database> db_1 = Database::Create(temp_dir.path());
  db_1->set_clock(scoped_ptr<Database::Clock>(clock_1));
  db_1->AddStateValue("test", "test");
  ASSERT_TRUE(db_1.get());
  ASSERT_TRUE(db_1->Close());

  TestingClock* clock_2 = new TestingClock(*clock_1);
  base::Time mid_time = clock_2->GetTime();
  scoped_ptr<Database> db_2 = Database::Create(temp_dir.path());
  db_2->set_clock(scoped_ptr<Database::Clock>(clock_2));
  db_2->AddStateValue("test", "test");
  ASSERT_TRUE(db_2.get());
  ASSERT_TRUE(db_2->Close());

  TestingClock* clock_3 = new TestingClock(*clock_2);
  base::Time end_time = clock_3->GetTime();
  scoped_ptr<Database> db_3 = Database::Create(temp_dir.path());
  db_3->set_clock(scoped_ptr<Database::Clock>(clock_3));
  db_3->AddStateValue("test", "test");
  ASSERT_TRUE(db_3.get());

  std::vector<TimeRange> active_interval = db_3->GetActiveIntervals(start_time,
                                                                    end_time);
  ASSERT_EQ(active_interval.size(), 2u);
  ASSERT_TRUE(active_interval[0].start > start_time &&
              active_interval[0].end < mid_time);
  ASSERT_TRUE(active_interval[1].start > mid_time &&
              active_interval[1].end < end_time);
}

TEST(PerformanceMonitorDatabaseSetupTest,
     ActiveIntervalRetrievalDuringActiveInterval) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  TestingClock* clock = new TestingClock();
  scoped_ptr<Database> db = Database::Create(temp_dir.path());
  db->set_clock(scoped_ptr<Database::Clock>(clock));
  db->AddStateValue("test", "test");
  base::Time start_time = clock->GetTime();
  db->AddStateValue("test", "test");
  base::Time end_time = clock->GetTime();
  db->AddStateValue("test", "test");

  std::vector<TimeRange> active_interval = db->GetActiveIntervals(start_time,
                                                                  end_time);
  ASSERT_EQ(1u, active_interval.size());
  EXPECT_LT(active_interval[0].start, start_time);
  EXPECT_GT(active_interval[0].end, start_time);
}

TEST(PerformanceMonitorDatabaseSetupTest,
     ActiveIntervalRetrievalAfterActiveInterval) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  TestingClock* clock = new TestingClock();
  scoped_ptr<Database> db = Database::Create(temp_dir.path());
  db->set_clock(scoped_ptr<Database::Clock>(clock));
  db->AddStateValue("test", "test");

  base::Time start_time = clock->GetTime();
  base::Time end_time = clock->GetTime();
  std::vector<TimeRange> active_interval = db->GetActiveIntervals(start_time,
                                                                  end_time);
  EXPECT_TRUE(active_interval.empty());
}

////// PerformanceMonitorDatabaseEventTests ////////////////////////////////////
TEST_F(PerformanceMonitorDatabaseEventTest, GetAllEvents) {
  Database::EventVector events = db_->GetEvents();
  ASSERT_EQ(4u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(install_event_1_->data()));
  EXPECT_TRUE(events[1]->data()->Equals(install_event_2_->data()));
  EXPECT_TRUE(events[2]->data()->Equals(uninstall_event_1_->data()));
  EXPECT_TRUE(events[3]->data()->Equals(uninstall_event_2_->data()));
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetMaxMetric) {
  Metric stat;
  EXPECT_EQ(0.0, db_->GetMaxStatsForActivityAndMetric(activity_,
                                                      METRIC_PAGE_LOAD_TIME));

  EXPECT_EQ(1000000,
            db_->GetMaxStatsForActivityAndMetric(METRIC_PRIVATE_MEMORY_USAGE));

  db_->AddMetric(kProcessChromeAggregate, METRIC_PRIVATE_MEMORY_USAGE,
                 std::string("99"));
  EXPECT_EQ(1000000,
            db_->GetMaxStatsForActivityAndMetric(METRIC_PRIVATE_MEMORY_USAGE));

  db_->AddMetric(kProcessChromeAggregate, METRIC_PRIVATE_MEMORY_USAGE,
                 std::string("6000000"));
  EXPECT_EQ(6000000,
            db_->GetMaxStatsForActivityAndMetric(METRIC_PRIVATE_MEMORY_USAGE));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetAllEventTypes) {
  Database::EventTypeSet types = db_->GetEventTypes();
  ASSERT_EQ(2u, types.size());
  ASSERT_EQ(1u, types.count(EVENT_EXTENSION_INSTALL));
  ASSERT_EQ(1u, types.count(EVENT_EXTENSION_UNINSTALL));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetEventInTimeRange) {
  base::Time start_time = clock_->GetTime();
  scoped_ptr<Event> crash_event = util::CreateRendererFreezeEvent(
      clock_->GetTime(), "chrome://freeze");
  db_->AddEvent(*crash_event.get());
  Database::EventVector events = db_->GetEvents(start_time, clock_->GetTime());
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(crash_event->data()));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetInstallEvents) {
  Database::EventVector events = db_->GetEvents(EVENT_EXTENSION_INSTALL);
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(install_event_1_->data()));
  EXPECT_TRUE(events[1]->data()->Equals(install_event_2_->data()));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetUnusedEventType) {
  Database::EventVector events = db_->GetEvents(EVENT_EXTENSION_DISABLE);
  ASSERT_TRUE(events.empty());
  events = db_->GetEvents(EVENT_EXTENSION_DISABLE, clock_->GetTime(),
                          clock_->GetTime());
  ASSERT_TRUE(events.empty());
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetEventsTimeRange) {
  base::Time start_time = clock_->GetTime();
  scoped_ptr<Event> new_install_event =
      util::CreateExtensionEvent(EVENT_EXTENSION_INSTALL, clock_->GetTime(),
                                 "c", "test extension", "http://foo.com",
                                 static_cast<int>(Extension::LOAD), "0.1",
                                 "Test Test");
  scoped_ptr<Event> new_uninstall_event =
      util::CreateExtensionEvent(EVENT_EXTENSION_UNINSTALL, clock_->GetTime(),
                                 "c", "test extension", "http://foo.com",
                                 static_cast<int>(Extension::LOAD), "0.1",
                                 "Test Test");
  base::Time end_time = clock_->GetTime();
  db_->AddEvent(*new_install_event.get());
  db_->AddEvent(*new_uninstall_event.get());
  Database::EventVector events = db_->GetEvents(start_time, end_time);
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(new_install_event->data()));
  EXPECT_TRUE(events[1]->data()->Equals(new_uninstall_event->data()));
  events = db_->GetEvents(
      EVENT_EXTENSION_INSTALL, start_time, end_time);
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(new_install_event->data()));
}

////// PerformanceMonitorDatabaseMetricTests ///////////////////////////////////
TEST_F(PerformanceMonitorDatabaseMetricTest, GetActiveMetrics) {
  Database::MetricTypeSet active_metrics =
    db_->GetActiveMetrics(base::Time(), clock_->GetTime());

  Database::MetricTypeSet expected_metrics;
  expected_metrics.insert(METRIC_CPU_USAGE);
  expected_metrics.insert(METRIC_PRIVATE_MEMORY_USAGE);
  EXPECT_EQ(expected_metrics, active_metrics);
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetRecentMetric) {
  Metric stat;
  ASSERT_TRUE(db_->GetRecentStatsForActivityAndMetric(activity_,
      METRIC_PRIVATE_MEMORY_USAGE, &stat));
  EXPECT_EQ(3000000, stat.value);

  ASSERT_TRUE(db_->GetRecentStatsForActivityAndMetric(METRIC_CPU_USAGE, &stat));
  EXPECT_EQ(50.5, stat.value);

  ScopedTempDir dir;
  CHECK(dir.CreateUniqueTempDir());
  scoped_ptr<Database> db = Database::Create(dir.path());
  CHECK(db.get());
  db->set_clock(scoped_ptr<Database::Clock>(new TestingClock()));
  EXPECT_FALSE(db->GetRecentStatsForActivityAndMetric(METRIC_CPU_USAGE, &stat));
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetState) {
  std::string key("version");
  std::string value("1.0.0.0.1");
  db_->AddStateValue(key, value);
  EXPECT_EQ(db_->GetStateValue(key), value);
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetStateOverride) {
  std::string key("version");
  std::string value_1("1.0.0.0.0");
  std::string value_2("1.0.0.0.1");
  db_->AddStateValue(key, value_1);
  db_->AddStateValue(key, value_2);
  EXPECT_EQ(db_->GetStateValue(key), value_2);
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetStatsForActivityAndMetric) {
  Database::MetricVector stats = db_->GetStatsForActivityAndMetric(
      activity_, METRIC_CPU_USAGE, base::Time(), clock_->GetTime());
  ASSERT_EQ(1u, stats.size());
  EXPECT_EQ(13.1, stats[0].value);
  base::Time before = clock_->GetTime();
  db_->AddMetric(activity_, METRIC_CPU_USAGE, std::string("18"));
  stats = db_->GetStatsForActivityAndMetric(activity_, METRIC_CPU_USAGE,
                                            before, clock_->GetTime());
  ASSERT_EQ(1u, stats.size());
  EXPECT_EQ(18, stats[0].value);
  stats = db_->GetStatsForActivityAndMetric(activity_, METRIC_CPU_USAGE);
  ASSERT_EQ(2u, stats.size());
  EXPECT_EQ(13.1, stats[0].value);
  EXPECT_EQ(18, stats[1].value);
  stats = db_->GetStatsForActivityAndMetric(METRIC_PRIVATE_MEMORY_USAGE);
  ASSERT_EQ(1u, stats.size());
  EXPECT_EQ(1000000, stats[0].value);
  stats = db_->GetStatsForActivityAndMetric(activity_, METRIC_CPU_USAGE,
                                            clock_->GetTime(),
                                            clock_->GetTime());
  EXPECT_TRUE(stats.empty());
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetStatsForMetricByActivity) {
  Database::MetricVectorMap stats_map = db_->GetStatsForMetricByActivity(
      METRIC_CPU_USAGE, base::Time(), clock_->GetTime());
  ASSERT_EQ(2u, stats_map.size());
  linked_ptr<Database::MetricVector> stats = stats_map[activity_];
  ASSERT_EQ(1u, stats->size());
  EXPECT_EQ(13.1, stats->at(0).value);
  stats = stats_map[kProcessChromeAggregate];
  ASSERT_EQ(1u, stats->size());
  EXPECT_EQ(50.5, stats->at(0).value);
  stats_map = db_->GetStatsForMetricByActivity(
      METRIC_CPU_USAGE, clock_->GetTime(), clock_->GetTime());
  EXPECT_EQ(0u, stats_map.size());
  base::Time before = clock_->GetTime();
  db_->AddMetric(activity_, METRIC_CPU_USAGE, std::string("18"));
  stats_map = db_->GetStatsForMetricByActivity(METRIC_CPU_USAGE, before,
                                               clock_->GetTime());
  ASSERT_EQ(1u, stats_map.size());
  stats = stats_map[activity_];
  ASSERT_EQ(1u, stats->size());
  EXPECT_EQ(18, stats->at(0).value);
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetFullRange) {
  db_->AddMetric(kProcessChromeAggregate, METRIC_CPU_USAGE, std::string("3.4"));
  db_->AddMetric(kProcessChromeAggregate, METRIC_CPU_USAGE, std::string("21"));
  Database::MetricVector stats =
      db_->GetStatsForActivityAndMetric(METRIC_CPU_USAGE);
  ASSERT_EQ(3u, stats.size());
  ASSERT_EQ(50.5, stats[0].value);
  ASSERT_EQ(3.4, stats[1].value);
  ASSERT_EQ(21, stats[2].value);
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetRange) {
  base::Time start = clock_->GetTime();
  db_->AddMetric(kProcessChromeAggregate, METRIC_CPU_USAGE, std::string("3"));
  db_->AddMetric(kProcessChromeAggregate, METRIC_CPU_USAGE, std::string("9"));
  base::Time end = clock_->GetTime();
  db_->AddMetric(kProcessChromeAggregate, METRIC_CPU_USAGE, std::string("21"));
  Database::MetricVector stats =
      db_->GetStatsForActivityAndMetric(METRIC_CPU_USAGE, start, end);
  ASSERT_EQ(2u, stats.size());
  ASSERT_EQ(3, stats[0].value);
  ASSERT_EQ(9, stats[1].value);
}

}  // namespace performance_monitor
