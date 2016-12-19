// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/common/variations_util.h"

#include <map>
#include <string>
#include <vector>

#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_scheduler_util {

namespace {

using StandbyThreadPolicy =
    base::SchedulerWorkerPoolParams::StandbyThreadPolicy;
using ThreadPriority = base::ThreadPriority;

std::vector<SchedulerImmutableWorkerPoolParams> GetImmutableWorkerPoolParams() {
  std::vector<SchedulerImmutableWorkerPoolParams> constant_worker_pool_params;
  constant_worker_pool_params.emplace_back("Background",
                                           ThreadPriority::BACKGROUND);
  constant_worker_pool_params.emplace_back("BackgroundFileIO",
                                           ThreadPriority::BACKGROUND);
  constant_worker_pool_params.emplace_back("Foreground",
                                           ThreadPriority::NORMAL);
  constant_worker_pool_params.emplace_back("ForegroundFileIO",
                                           ThreadPriority::NORMAL);
  return constant_worker_pool_params;
}

}  // namespace

TEST(TaskSchedulerUtilVariationsUtilTest, OrderingParams5) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "1;1;1;0;42";
  variation_params["BackgroundFileIO"] = "2;2;1;0;52";
  variation_params["Foreground"] = "4;4;1;0;62";
  variation_params["ForegroundFileIO"] = "8;8;1;0;72";

  auto params_vector =
      GetWorkerPoolParams(GetImmutableWorkerPoolParams(), variation_params);
  ASSERT_EQ(4U, params_vector.size());

  EXPECT_EQ("Background", params_vector[0].name());
  EXPECT_EQ(ThreadPriority::BACKGROUND, params_vector[0].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[0].standby_thread_policy());
  EXPECT_EQ(1U, params_vector[0].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(42),
            params_vector[0].suggested_reclaim_time());

  EXPECT_EQ("BackgroundFileIO", params_vector[1].name());
  EXPECT_EQ(ThreadPriority::BACKGROUND, params_vector[1].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[1].standby_thread_policy());
  EXPECT_EQ(2U, params_vector[1].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(52),
            params_vector[1].suggested_reclaim_time());

  EXPECT_EQ("Foreground", params_vector[2].name());
  EXPECT_EQ(ThreadPriority::NORMAL, params_vector[2].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[2].standby_thread_policy());
  EXPECT_EQ(4U, params_vector[2].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(62),
            params_vector[2].suggested_reclaim_time());

  EXPECT_EQ("ForegroundFileIO", params_vector[3].name());
  EXPECT_EQ(ThreadPriority::NORMAL, params_vector[3].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[3].standby_thread_policy());
  EXPECT_EQ(8U, params_vector[3].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(72),
            params_vector[3].suggested_reclaim_time());
}

TEST(TaskSchedulerUtilVariationsUtilTest, OrderingParams6) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "1;1;1;0;42;lazy";
  variation_params["BackgroundFileIO"] = "2;2;1;0;52;one";
  variation_params["Foreground"] = "4;4;1;0;62;lazy";
  variation_params["ForegroundFileIO"] = "8;8;1;0;72;one";

  auto params_vector =
      GetWorkerPoolParams(GetImmutableWorkerPoolParams(), variation_params);
  ASSERT_EQ(4U, params_vector.size());

  EXPECT_EQ("Background", params_vector[0].name());
  EXPECT_EQ(ThreadPriority::BACKGROUND, params_vector[0].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::LAZY,
            params_vector[0].standby_thread_policy());
  EXPECT_EQ(1U, params_vector[0].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(42),
            params_vector[0].suggested_reclaim_time());

  EXPECT_EQ("BackgroundFileIO", params_vector[1].name());
  EXPECT_EQ(ThreadPriority::BACKGROUND, params_vector[1].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[1].standby_thread_policy());
  EXPECT_EQ(2U, params_vector[1].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(52),
            params_vector[1].suggested_reclaim_time());

  EXPECT_EQ("Foreground", params_vector[2].name());
  EXPECT_EQ(ThreadPriority::NORMAL, params_vector[2].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::LAZY,
            params_vector[2].standby_thread_policy());
  EXPECT_EQ(4U, params_vector[2].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(62),
            params_vector[2].suggested_reclaim_time());

  EXPECT_EQ("ForegroundFileIO", params_vector[3].name());
  EXPECT_EQ(ThreadPriority::NORMAL, params_vector[3].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[3].standby_thread_policy());
  EXPECT_EQ(8U, params_vector[3].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(72),
            params_vector[3].suggested_reclaim_time());
}

TEST(TaskSchedulerUtilVariationsUtilTest, NoData) {
  EXPECT_TRUE(GetWorkerPoolParams(GetImmutableWorkerPoolParams(),
                                  std::map<std::string, std::string>())
                  .empty());
}

TEST(TaskSchedulerUtilVariationsUtilTest, IncompleteParameters) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "1;1;1;0";
  variation_params["BackgroundFileIO"] = "2;2;1;0";
  variation_params["Foreground"] = "4;4;1;0";
  variation_params["ForegroundFileIO"] = "8;8;1;0";
  EXPECT_TRUE(
      GetWorkerPoolParams(GetImmutableWorkerPoolParams(), variation_params)
          .empty());
}

TEST(TaskSchedulerUtilVariationsUtilTest, InvalidParameters) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "a;b;c;d;e";
  variation_params["BackgroundFileIO"] = "a;b;c;d;e";
  variation_params["Foreground"] = "a;b;c;d;e";
  variation_params["ForegroundFileIO"] = "a;b;c;d;e";
  EXPECT_TRUE(
      GetWorkerPoolParams(GetImmutableWorkerPoolParams(), variation_params)
          .empty());
}

}  // namespace task_scheduler_util
