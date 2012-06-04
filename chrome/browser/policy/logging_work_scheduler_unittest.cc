// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/logging_work_scheduler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace policy {

class LoggingWorkSchedulerTest : public testing::Test {
 public:
  LoggingWorkSchedulerTest()
      : ui_thread_(BrowserThread::UI, &loop_) {
  }

  virtual ~LoggingWorkSchedulerTest() {
  }

  virtual void TearDown() {
    scheduler1_.reset();
    scheduler2_.reset();
    logger_.reset();
  }

 protected:
  void Callback1() {
    logger_->RegisterEvent();
    if (count1_ > 0) {
      count1_--;
      scheduler1_->PostDelayedWork(
          base::Bind(&LoggingWorkSchedulerTest::Callback1,
                     base::Unretained(this)), delay1_);
    }
  }

  void Callback2() {
    logger_->RegisterEvent();
    if (count2_ > 0) {
      count2_--;
      scheduler2_->PostDelayedWork(
          base::Bind(&LoggingWorkSchedulerTest::Callback2,
                     base::Unretained(this)), delay2_);
    }
 }

 protected:
  scoped_ptr<EventLogger> logger_;

  // The first scheduler will fire |count1_| events with |delay1_| pauses
  // between each.
  scoped_ptr<LoggingWorkScheduler> scheduler1_;
  int count1_;
  int delay1_;

  // The second scheduler will fire |count2_| events with |delay2_| pauses
  // between each.
  scoped_ptr<LoggingWorkScheduler> scheduler2_;
  int count2_;
  int delay2_;

  MessageLoop loop_;

 private:
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(LoggingWorkSchedulerTest);
};

TEST_F(LoggingWorkSchedulerTest, LoggerTest) {
  logger_.reset(new EventLogger);
  scheduler1_.reset(new LoggingWorkScheduler(logger_.get()));
  scheduler2_.reset(new LoggingWorkScheduler(logger_.get()));

  // Configure the first scheduler to fire at 0, 30, 60, 90, 120.
  count1_ = 4;
  delay1_ = 30;

  // Configure the first scheduler to fire at 0, 40, 80, 120.
  count2_ = 3;
  delay2_ = 40;

  Callback1();
  Callback2();
  loop_.RunAllPending();

  std::vector<int64> events;
  logger_->Swap(&events);

  EXPECT_EQ(9u, events.size());
  EXPECT_EQ(0, events[0]);
  EXPECT_EQ(0, events[1]);
  EXPECT_EQ(30, events[2]);
  EXPECT_EQ(40, events[3]);
  EXPECT_EQ(60, events[4]);
  EXPECT_EQ(80, events[5]);
  EXPECT_EQ(90, events[6]);
  EXPECT_EQ(120, events[7]);
  EXPECT_EQ(120, events[8]);

  EXPECT_EQ(0, EventLogger::CountEvents(events, 0, 0));
  EXPECT_EQ(2, EventLogger::CountEvents(events, 0, 1));
  EXPECT_EQ(4, EventLogger::CountEvents(events, 30, 51));
  EXPECT_EQ(7, EventLogger::CountEvents(events, 0, 120));
  EXPECT_EQ(7, EventLogger::CountEvents(events, 1, 120));
}

}  // namespace policy
