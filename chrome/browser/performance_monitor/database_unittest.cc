// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/performance_monitor/database.h"
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
    return base::Time::FromInternalValue(counter_++);
  }
 private:
  int64 counter_;
};

class PerformanceMonitorDatabaseEventTest : public ::testing::Test {
 protected:
  PerformanceMonitorDatabaseEventTest() {
    clock_ = new TestingClock();
    file_util::CreateNewTempDirectory(FilePath::StringType(), &temp_path_);
    db_ = Database::Create(temp_path_);
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
  FilePath temp_path_;
  scoped_ptr<Event> install_event_1_;
  scoped_ptr<Event> install_event_2_;
  scoped_ptr<Event> uninstall_event_1_;
  scoped_ptr<Event> uninstall_event_2_;

 private:
  void InitEvents() {
    install_event_1_ = util::CreateExtensionInstallEvent(
        clock_->GetTime(), "a", "extension 1", "http://foo.com",
        static_cast<int>(Extension::LOAD), "0.1", "Test Test");
    install_event_2_ = util::CreateExtensionInstallEvent(
        clock_->GetTime(), "b", "extension 2", "http://bar.com",
        static_cast<int>(Extension::LOAD), "0.1", "Test Test");
    uninstall_event_1_ = util::CreateExtensionUninstallEvent(
        clock_->GetTime(), "a", "extension 1", "http://foo.com",
        static_cast<int>(Extension::LOAD), "0.1", "Test Test");
    uninstall_event_2_ = util::CreateExtensionUninstallEvent(
        clock_->GetTime(), "b", "extension 2", "http://bar.com",
        static_cast<int>(Extension::LOAD), "0.1", "Test Test");
  }
};

class PerformanceMonitorDatabaseMetricTest : public ::testing::Test {
 protected:
  PerformanceMonitorDatabaseMetricTest() {
    clock_ = new TestingClock();
    file_util::CreateNewTempDirectory(FilePath::StringType(), &temp_path_);
    db_ = Database::Create(temp_path_);
    CHECK(db_.get());
    db_->set_clock(scoped_ptr<Database::Clock>(clock_));
    activity_ = std::string("A");
    cpu_metric_ = std::string("CPU");
    mem_metric_ = std::string("MEM");
    fun_metric_ = std::string("FUN");
    cpu_metric_details_ = MetricDetails(cpu_metric_, "Testing CPU metric.");
    mem_metric_details_ = MetricDetails(mem_metric_, "Testing MEM metric.");
    fun_metric_details_ = MetricDetails(fun_metric_, "Testing FUN metric.");
  }

  void SetUp() {
    ASSERT_TRUE(db_.get());
    PopulateDB();
  }

  void PopulateDB() {
    db_->AddMetricDetails(cpu_metric_details_);
    db_->AddMetricDetails(mem_metric_details_);
    db_->AddMetricDetails(fun_metric_details_);
    db_->AddMetric(kProcessChromeAggregate, cpu_metric_, std::string("50.5"));
    db_->AddMetric(activity_, cpu_metric_, std::string("13.1"));
    db_->AddMetric(kProcessChromeAggregate, mem_metric_, std::string("100000"));
    db_->AddMetric(activity_, mem_metric_, std::string("20000"));
    db_->AddMetric(kProcessChromeAggregate, fun_metric_, std::string("3.14"));
    db_->AddMetric(activity_, fun_metric_, std::string("1.68"));
  }

  scoped_ptr<Database> db_;
  Database::Clock* clock_;
  FilePath temp_path_;
  std::string activity_;
  std::string cpu_metric_;
  std::string mem_metric_;
  std::string fun_metric_;
  MetricDetails cpu_metric_details_;
  MetricDetails mem_metric_details_;
  MetricDetails fun_metric_details_;
};

////// PerformanceMonitorDatabaseSetupTests ////////////////////////////////////
TEST(PerformanceMonitorDatabaseSetupTest, OpenClose) {
  FilePath alternate_path;
  file_util::CreateNewTempDirectory(FilePath::StringType(), &alternate_path);
  scoped_ptr<Database> db = Database::Create(alternate_path);
  ASSERT_TRUE(db.get());
  ASSERT_TRUE(db->Close());
}

TEST(PerformanceMonitorDatabaseSetupTest, ActiveInterval) {
  FilePath alternate_path;
  file_util::CreateNewTempDirectory(FilePath::StringType(), &alternate_path);

  TestingClock* clock_1 = new TestingClock();
  base::Time start_time = clock_1->GetTime();

  scoped_ptr<Database> db_1 = Database::Create(alternate_path);
  db_1->set_clock(scoped_ptr<Database::Clock>(clock_1));
  db_1->AddStateValue("test", "test");
  ASSERT_TRUE(db_1.get());
  ASSERT_TRUE(db_1->Close());

  TestingClock* clock_2 = new TestingClock(*clock_1);
  base::Time mid_time = clock_2->GetTime();
  scoped_ptr<Database> db_2 = Database::Create(alternate_path);
  db_2->set_clock(scoped_ptr<Database::Clock>(clock_2));
  db_2->AddStateValue("test", "test");
  ASSERT_TRUE(db_2.get());
  ASSERT_TRUE(db_2->Close());

  TestingClock* clock_3 = new TestingClock(*clock_2);
  base::Time end_time = clock_3->GetTime();
  scoped_ptr<Database> db_3 = Database::Create(alternate_path);
  db_3->set_clock(scoped_ptr<Database::Clock>(clock_3));
  db_3->AddStateValue("test", "test");
  ASSERT_TRUE(db_3.get());

  std::vector<TimeRange> active_interval = db_3->GetActiveIntervals(start_time,
                                                                    end_time);
  ASSERT_EQ(active_interval.size(), static_cast<size_t>(2));
  ASSERT_TRUE(active_interval[0].start > start_time &&
              active_interval[0].end < mid_time);
  ASSERT_TRUE(active_interval[1].start > mid_time &&
              active_interval[1].end < end_time);
}

////// PerformanceMonitorDatabaseEventTests ////////////////////////////////////
TEST_F(PerformanceMonitorDatabaseEventTest, GetAllEvents) {
  std::vector<linked_ptr<Event> > events = db_->GetEvents();
  ASSERT_EQ(4u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(install_event_1_->data()));
  EXPECT_TRUE(events[1]->data()->Equals(install_event_2_->data()));
  EXPECT_TRUE(events[2]->data()->Equals(uninstall_event_1_->data()));
  EXPECT_TRUE(events[3]->data()->Equals(uninstall_event_2_->data()));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetAllEventTypes) {
  std::set<EventType> types = db_->GetEventTypes();
  ASSERT_EQ(2u, types.size());
  ASSERT_EQ(1u, types.count(EVENT_EXTENSION_INSTALL));
  ASSERT_EQ(1u, types.count(EVENT_EXTENSION_UNINSTALL));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetEventInTimeRange) {
  base::Time start_time = clock_->GetTime();
  scoped_ptr<Event> crash_event = util::CreateRendererFreezeEvent(
      clock_->GetTime(), "chrome://freeze");
  db_->AddEvent(*crash_event.get());
  std::vector<linked_ptr<Event> > events =
      db_->GetEvents(start_time, clock_->GetTime());
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(crash_event->data()));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetInstallEvents) {
  std::vector<linked_ptr<Event> > events =
      db_->GetEvents(EVENT_EXTENSION_INSTALL);
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(install_event_1_->data()));
  EXPECT_TRUE(events[1]->data()->Equals(install_event_2_->data()));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetUnusedEventType) {
  std::vector<linked_ptr<Event> > events =
      db_->GetEvents(EVENT_EXTENSION_UNLOAD);
  ASSERT_TRUE(events.empty());
  events = db_->GetEvents(EVENT_EXTENSION_UNLOAD, clock_->GetTime(),
                          clock_->GetTime());
  ASSERT_TRUE(events.empty());
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetEventsTimeRange) {
  base::Time start_time = clock_->GetTime();
  scoped_ptr<Event> new_install_event =
      util::CreateExtensionInstallEvent(
      clock_->GetTime(), "c", "test extension", "http://foo.com",
      static_cast<int>(Extension::LOAD), "0.1", "Test Test");
  scoped_ptr<Event> new_uninstall_event =
      util::CreateExtensionUninstallEvent(
      clock_->GetTime(), "c", "test extension", "http://foo.com",
      static_cast<int>(Extension::LOAD), "0.1", "Test Test");
  base::Time end_time = clock_->GetTime();
  db_->AddEvent(*new_install_event.get());
  db_->AddEvent(*new_uninstall_event.get());
  std::vector<linked_ptr<Event> > events =
      db_->GetEvents(start_time, end_time);
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
  std::vector<MetricDetails> metrics =
    db_->GetActiveMetrics(base::Time(), clock_->GetTime());
  ASSERT_EQ(3u, metrics.size());
  std::vector<MetricDetails>::iterator it;
  for (it = metrics.begin(); it != metrics.end(); ++it) {
    if (it->name == cpu_metric_details_.name)
      EXPECT_EQ(it->description, cpu_metric_details_.description);
    else if (it->name == mem_metric_details_.name)
      EXPECT_EQ(it->description, mem_metric_details_.description);
    else if (it->name == fun_metric_details_.name)
      EXPECT_EQ(it->description, fun_metric_details_.description);
    else
      NOTREACHED();
  }
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
  Database::MetricInfoVector stats = db_->GetStatsForActivityAndMetric(
      activity_, cpu_metric_, base::Time(), clock_->GetTime());
  ASSERT_EQ(1u, stats.size());
  EXPECT_EQ(13.1, stats[0].value);
  base::Time before = clock_->GetTime();
  db_->AddMetric(activity_, cpu_metric_, std::string("18"));
  stats = db_->GetStatsForActivityAndMetric(activity_, cpu_metric_,
                                            before, clock_->GetTime());
  ASSERT_EQ(1u, stats.size());
  EXPECT_EQ(18, stats[0].value);
  stats = db_->GetStatsForActivityAndMetric(activity_, cpu_metric_);
  ASSERT_EQ(2u, stats.size());
  EXPECT_EQ(13.1, stats[0].value);
  EXPECT_EQ(18, stats[1].value);
  stats = db_->GetStatsForActivityAndMetric(mem_metric_);
  ASSERT_EQ(1u, stats.size());
  EXPECT_EQ(100000, stats[0].value);
  stats = db_->GetStatsForActivityAndMetric(activity_, cpu_metric_,
                                            clock_->GetTime(),
                                            clock_->GetTime());
  EXPECT_TRUE(stats.empty());
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetStatsForMetricByActivity) {
  Database::MetricVectorMap stats_map = db_->GetStatsForMetricByActivity(
      cpu_metric_, base::Time(), clock_->GetTime());
  ASSERT_EQ(2u, stats_map.size());
  linked_ptr<Database::MetricInfoVector> stats = stats_map[activity_];
  ASSERT_EQ(1u, stats->size());
  EXPECT_EQ(13.1, stats->at(0).value);
  stats = stats_map[kProcessChromeAggregate];
  ASSERT_EQ(1u, stats->size());
  EXPECT_EQ(50.5, stats->at(0).value);
  stats_map = db_->GetStatsForMetricByActivity(cpu_metric_, clock_->GetTime(),
                                               clock_->GetTime());
  EXPECT_EQ(0u, stats_map.size());
  base::Time before = clock_->GetTime();
  db_->AddMetric(activity_, cpu_metric_, std::string("18"));
  stats_map = db_->GetStatsForMetricByActivity(cpu_metric_, before,
                                               clock_->GetTime());
  ASSERT_EQ(1u, stats_map.size());
  stats = stats_map[activity_];
  ASSERT_EQ(1u, stats->size());
  EXPECT_EQ(18, stats->at(0).value);
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetFullRange) {
  db_->AddMetric(kProcessChromeAggregate, cpu_metric_, std::string("3.4"));
  db_->AddMetric(kProcessChromeAggregate, cpu_metric_, std::string("21"));
  Database::MetricInfoVector stats =
      db_->GetStatsForActivityAndMetric(cpu_metric_);
  ASSERT_EQ(3u, stats.size());
  ASSERT_EQ(50.5, stats[0].value);
  ASSERT_EQ(3.4, stats[1].value);
  ASSERT_EQ(21, stats[2].value);
}

TEST_F(PerformanceMonitorDatabaseMetricTest, GetRange) {
  base::Time start = clock_->GetTime();
  db_->AddMetric(kProcessChromeAggregate, cpu_metric_, std::string("3"));
  db_->AddMetric(kProcessChromeAggregate, cpu_metric_, std::string("9"));
  base::Time end = clock_->GetTime();
  db_->AddMetric(kProcessChromeAggregate, cpu_metric_, std::string("21"));
  Database::MetricInfoVector stats =
      db_->GetStatsForActivityAndMetric(cpu_metric_, start, end);
  ASSERT_EQ(2u, stats.size());
  ASSERT_EQ(3, stats[0].value);
  ASSERT_EQ(9, stats[1].value);
}

}  // namespace performance_monitor
