// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/vsync_time_source.h"

#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakeVSyncProvider : public VSyncProvider {
 public:
  FakeVSyncProvider() : client_(NULL) {}

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
  VSyncTimeSourceTest() : timer_(VSyncTimeSource::Create(&provider_)) {
    timer_->SetClient(&client_);
  }

 protected:
  FakeTimeSourceClient client_;
  FakeVSyncProvider provider_;
  scoped_refptr<VSyncTimeSource> timer_;
};

TEST_F(VSyncTimeSourceTest, TaskPostedAndTickCalled) {
  EXPECT_FALSE(provider_.IsVSyncNotificationEnabled());

  timer_->SetActive(true);
  EXPECT_TRUE(provider_.IsVSyncNotificationEnabled());

  base::TimeTicks frame_time = base::TimeTicks::Now();
  provider_.Trigger(frame_time);
  EXPECT_TRUE(client_.TickCalled());
  EXPECT_EQ(timer_->LastTickTime(), frame_time);
}

TEST_F(VSyncTimeSourceTest, NotificationDisabledLazily) {
  base::TimeTicks frame_time = base::TimeTicks::Now();

  // Enable timer and trigger sync once.
  timer_->SetActive(true);
  EXPECT_TRUE(provider_.IsVSyncNotificationEnabled());
  provider_.Trigger(frame_time);
  EXPECT_TRUE(client_.TickCalled());

  // Disabling the timer should not disable vsync notification immediately.
  client_.Reset();
  timer_->SetActive(false);
  EXPECT_TRUE(provider_.IsVSyncNotificationEnabled());

  // At the next vsync the notification is disabled, but the timer isn't ticked.
  provider_.Trigger(frame_time);
  EXPECT_FALSE(provider_.IsVSyncNotificationEnabled());
  EXPECT_FALSE(client_.TickCalled());

  // The notification should not be disabled multiple times.
  provider_.RequestVSyncNotification(timer_.get());
  provider_.Trigger(frame_time);
  EXPECT_TRUE(provider_.IsVSyncNotificationEnabled());
  EXPECT_FALSE(client_.TickCalled());
}

TEST_F(VSyncTimeSourceTest, ValidNextTickTime) {
  base::TimeTicks frame_time = base::TimeTicks::Now();
  base::TimeDelta interval = base::TimeDelta::FromSeconds(1);

  ASSERT_EQ(timer_->NextTickTime(), base::TimeTicks());

  timer_->SetActive(true);
  provider_.Trigger(frame_time);
  ASSERT_EQ(timer_->NextTickTime(), frame_time);

  timer_->SetTimebaseAndInterval(frame_time, interval);
  ASSERT_EQ(timer_->NextTickTime(), frame_time + interval);
}

}  // namespace
}  // namespace cc
