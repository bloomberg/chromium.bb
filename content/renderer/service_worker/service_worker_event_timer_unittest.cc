// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_event_timer.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerEventTimerTest : public testing::Test {
 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    message_loop_.SetTaskRunner(task_runner_);
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

TEST_F(ServiceWorkerEventTimerTest, IdleTimer) {
  bool is_idle = false;
  ServiceWorkerEventTimer timer(base::BindRepeating(
      [](bool* out_is_idle) { *out_is_idle = true; }, &is_idle));
  task_runner()->FastForwardBy(ServiceWorkerEventTimer::kIdleDelay +
                               base::TimeDelta::FromSeconds(1));
  // |idle_callback| should be fired since there is no event.
  EXPECT_TRUE(is_idle);

  is_idle = false;
  timer.StartEvent();
  task_runner()->FastForwardBy(ServiceWorkerEventTimer::kIdleDelay +
                               base::TimeDelta::FromSeconds(1));
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  timer.StartEvent();
  task_runner()->FastForwardBy(ServiceWorkerEventTimer::kIdleDelay +
                               base::TimeDelta::FromSeconds(1));
  // Nothing happens since there are two inflight events.
  EXPECT_FALSE(is_idle);

  timer.EndEvent();
  task_runner()->FastForwardBy(ServiceWorkerEventTimer::kIdleDelay +
                               base::TimeDelta::FromSeconds(1));
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  timer.EndEvent();
  task_runner()->FastForwardBy(ServiceWorkerEventTimer::kIdleDelay +
                               base::TimeDelta::FromSeconds(1));
  // |idle_callback| should be fired.
  EXPECT_TRUE(is_idle);
}

}  // namespace content
