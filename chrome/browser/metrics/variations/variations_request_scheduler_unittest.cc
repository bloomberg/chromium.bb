// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_request_scheduler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

namespace {

void DoNothing() {
}

}  // namespace

TEST(VariationsRequestSchedulerTest, ScheduleFetchShortly) {
  base::MessageLoopForUI message_loop_;

  const base::Closure task = base::Bind(&DoNothing);
  VariationsRequestScheduler scheduler(task);
  EXPECT_FALSE(scheduler.one_shot_timer_.IsRunning());

  scheduler.ScheduleFetchShortly();
  EXPECT_TRUE(scheduler.one_shot_timer_.IsRunning());
}

}  // namespace chrome_variations
