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
#include "testing/gtest/include/gtest/gtest.h"

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
}  // namespace performance_monitor
