// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/initialization/browser_util.h"

#include <vector>

#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_scheduler_util {
namespace initialization {

TEST(TaskSchedulerUtilBrowserUtilTest, CheckOrdering) {
  using StandbyThreadPolicy =
      base::SchedulerWorkerPoolParams::StandbyThreadPolicy;
  BrowserWorkerPoolsConfiguration config;
  config.background.standby_thread_policy = StandbyThreadPolicy::LAZY;
  config.background.threads = 1;
  config.background.detach_period = base::TimeDelta::FromSeconds(2);

  config.background_file_io.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.background_file_io.threads = 3;
  config.background_file_io.detach_period = base::TimeDelta::FromSeconds(4);

  config.foreground.standby_thread_policy = StandbyThreadPolicy::LAZY;
  config.foreground.threads = 5;
  config.foreground.detach_period = base::TimeDelta::FromSeconds(6);

  config.foreground_file_io.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.foreground_file_io.threads = 7;
  config.foreground_file_io.detach_period = base::TimeDelta::FromSeconds(8);

  const std::vector<base::SchedulerWorkerPoolParams> params_vector =
      BrowserWorkerPoolConfigurationToSchedulerWorkerPoolParams(config);

  ASSERT_EQ(4U, params_vector.size());

  EXPECT_EQ(StandbyThreadPolicy::LAZY,
            params_vector[0].standby_thread_policy());
  EXPECT_EQ(1U, params_vector[0].max_threads());
  EXPECT_EQ(base::TimeDelta::FromSeconds(2),
            params_vector[0].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[1].standby_thread_policy());
  EXPECT_EQ(3U, params_vector[1].max_threads());
  EXPECT_EQ(base::TimeDelta::FromSeconds(4),
            params_vector[1].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::LAZY,
            params_vector[2].standby_thread_policy());
  EXPECT_EQ(5U, params_vector[2].max_threads());
  EXPECT_EQ(base::TimeDelta::FromSeconds(6),
            params_vector[2].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[3].standby_thread_policy());
  EXPECT_EQ(7U, params_vector[3].max_threads());
  EXPECT_EQ(base::TimeDelta::FromSeconds(8),
            params_vector[3].suggested_reclaim_time());
}

}  // namespace initialization
}  // namespace task_scheduler_util
