// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/vsync_time_source.h"

#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakeVSyncProvider : public VSyncProvider {
 public:
  FakeVSyncProvider()
    : client_(NULL) {}

  // VSyncProvider implementation.
  virtual void RequestVSyncNotification(VSyncClient* client) OVERRIDE {
    client_ = client;
  }

  bool IsVSyncNotificationEnabled() const { return client_ != NULL; }

  void Trigger(base::TimeTicks frame_time) {
    if (client_)
      client_->DidVSync(frame_time);
  }

 private:
  VSyncClient* client_;
};

class VSyncTimeSourceTest : public testing::Test {
 public:
  VSyncTimeSourceTest()
      : timer_(VSyncTimeSource::create(&provider_)) {
    timer_->setClient(&client_);
  }

 protected:
  FakeTimeSourceClient client_;
  FakeVSyncProvider provider_;
  scoped_refptr<VSyncTimeSource> timer_;
};

TEST_F(VSyncTimeSourceTest, TaskPostedAndTickCalled)
{
  EXPECT_FALSE(provider_.IsVSyncNotificationEnabled());

  timer_->setActive(true);
  EXPECT_TRUE(provider_.IsVSyncNotificationEnabled());

  base::TimeTicks frame_time = base::TimeTicks::Now();
  provider_.Trigger(frame_time);
  EXPECT_TRUE(client_.tickCalled());
  EXPECT_EQ(timer_->lastTickTime(), frame_time);
}

TEST_F(VSyncTimeSourceTest, NotificationDisabledLazily)
{
  base::TimeTicks frame_time = base::TimeTicks::Now();

  // Enable timer and trigger sync once.
  timer_->setActive(true);
  EXPECT_TRUE(provider_.IsVSyncNotificationEnabled());
  provider_.Trigger(frame_time);
  EXPECT_TRUE(client_.tickCalled());

  // Disabling the timer should not disable vsync notification immediately.
  client_.reset();
  timer_->setActive(false);
  EXPECT_TRUE(provider_.IsVSyncNotificationEnabled());

  // At the next vsync the notification is disabled, but the timer isn't ticked.
  provider_.Trigger(frame_time);
  EXPECT_FALSE(provider_.IsVSyncNotificationEnabled());
  EXPECT_FALSE(client_.tickCalled());

  // The notification should not be disabled multiple times.
  provider_.RequestVSyncNotification(timer_.get());
  provider_.Trigger(frame_time);
  EXPECT_TRUE(provider_.IsVSyncNotificationEnabled());
  EXPECT_FALSE(client_.tickCalled());
}

TEST_F(VSyncTimeSourceTest, ValidNextTickTime)
{
  base::TimeTicks frame_time = base::TimeTicks::Now();
  base::TimeDelta interval = base::TimeDelta::FromSeconds(1);

  ASSERT_EQ(timer_->nextTickTime(), base::TimeTicks());

  timer_->setActive(true);
  provider_.Trigger(frame_time);
  ASSERT_EQ(timer_->nextTickTime(), frame_time);

  timer_->setTimebaseAndInterval(frame_time, interval);
  ASSERT_EQ(timer_->nextTickTime(), frame_time + interval);
}

}  // namespace
}  // namespace cc
