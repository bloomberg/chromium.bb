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
    ASSERT_TRUE(db_);
    PopulateDB();
  }

  void PopulateDB() {
    InitEvents();
    db_->AddEvent(*install_event_1_.get());
    db_->AddEvent(*install_event_2_.get());
    db_->AddEvent(*uninstall_event_1_.get());
    db_->AddEvent(*uninstall_event_2_.get());
  }

  scoped_refptr<Database> db_;
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

////// PerformanceMonitorDatabaseSetupTests ////////////////////////////////////
TEST(PerformanceMonitorDatabaseSetupTest, OpenCloseTest) {
  FilePath alternate_path;
  file_util::CreateNewTempDirectory(FilePath::StringType(), &alternate_path);
  scoped_refptr<Database> db = Database::Create(alternate_path);
  ASSERT_TRUE(db);
  ASSERT_TRUE(db->Close());
}

TEST(PerformanceMonitorDatabaseSetupTest, ActiveIntervalTest) {
  FilePath alternate_path;
  file_util::CreateNewTempDirectory(FilePath::StringType(), &alternate_path);

  TestingClock* clock_1 = new TestingClock();
  base::Time start_time = clock_1->GetTime();

  scoped_refptr<Database> db_1 = Database::Create(alternate_path);
  db_1->set_clock(scoped_ptr<Database::Clock>(clock_1));
  db_1->AddStateValue("test", "test");
  ASSERT_TRUE(db_1);
  ASSERT_TRUE(db_1->Close());

  TestingClock* clock_2 = new TestingClock(*clock_1);
  base::Time mid_time = clock_2->GetTime();
  scoped_refptr<Database> db_2 = Database::Create(alternate_path);
  db_2->set_clock(scoped_ptr<Database::Clock>(clock_2));
  db_2->AddStateValue("test", "test");
  ASSERT_TRUE(db_2);
  ASSERT_TRUE(db_2->Close());

  TestingClock* clock_3 = new TestingClock(*clock_2);
  base::Time end_time = clock_3->GetTime();
  scoped_refptr<Database> db_3 = Database::Create(alternate_path);
  db_3->set_clock(scoped_ptr<Database::Clock>(clock_3));
  db_3->AddStateValue("test", "test");
  ASSERT_TRUE(db_3);

  std::vector<TimeRange> active_interval = db_3->GetActiveIntervals(start_time,
                                                                    end_time);
  ASSERT_EQ(active_interval.size(), static_cast<size_t>(2));
  ASSERT_TRUE(active_interval[0].start > start_time &&
              active_interval[0].end < mid_time);
  ASSERT_TRUE(active_interval[1].start > mid_time &&
              active_interval[1].end < end_time);
}

////// PerformanceMonitorDatabaseEventTests ////////////////////////////////////
TEST_F(PerformanceMonitorDatabaseEventTest, GetAllEventsTest) {
  std::vector<linked_ptr<Event> > events = db_->GetEvents();
  ASSERT_EQ(4u, events.size());
  EXPECT_TRUE(events[0]->data()->Equals(install_event_1_->data()));
  EXPECT_TRUE(events[1]->data()->Equals(install_event_2_->data()));
  EXPECT_TRUE(events[2]->data()->Equals(uninstall_event_1_->data()));
  EXPECT_TRUE(events[3]->data()->Equals(uninstall_event_2_->data()));
}

TEST_F(PerformanceMonitorDatabaseEventTest, GetAllEventTypesTest) {
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
  events = db_->GetEvents(EVENT_EXTENSION_UNLOAD,
                             clock_->GetTime(),
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
}  // namespace performance_monitor
