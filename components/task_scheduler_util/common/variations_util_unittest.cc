// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/common/variations_util.h"

#include <map>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/task_scheduler/scheduler_worker_params.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/threading/platform_thread.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_scheduler_util {

namespace {

using StandbyThreadPolicy =
    base::SchedulerWorkerPoolParams::StandbyThreadPolicy;
using ThreadPriority = base::ThreadPriority;

#if !defined(OS_IOS)
constexpr char kFieldTrialName[] = "BrowserScheduler";
constexpr char kFieldTrialTestGroup[] = "Test";
constexpr char kTaskSchedulerVariationParamsSwitch[] =
    "task-scheduler-variation-params";
#endif  // !defined(OS_IOS)

std::vector<SchedulerImmutableWorkerPoolParams> GetImmutableWorkerPoolParams() {
  std::vector<SchedulerImmutableWorkerPoolParams> constant_worker_pool_params;
  constant_worker_pool_params.emplace_back("Background",
                                           ThreadPriority::BACKGROUND);
  constant_worker_pool_params.emplace_back("BackgroundFileIO",
                                           ThreadPriority::BACKGROUND);
  constant_worker_pool_params.emplace_back("Foreground",
                                           ThreadPriority::NORMAL);
  constant_worker_pool_params.emplace_back(
      "ForegroundFileIO", ThreadPriority::NORMAL,
      base::SchedulerBackwardCompatibility::INIT_COM_STA);
  return constant_worker_pool_params;
}

class TaskSchedulerUtilVariationsUtilTest : public testing::Test {
 public:
  TaskSchedulerUtilVariationsUtilTest() : field_trial_list_(nullptr) {}
  ~TaskSchedulerUtilVariationsUtilTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();
  }

 private:
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerUtilVariationsUtilTest);
};

}  // namespace

TEST_F(TaskSchedulerUtilVariationsUtilTest, OrderingParams5) {
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
  EXPECT_EQ(base::SchedulerBackwardCompatibility::DISABLED,
            params_vector[0].backward_compatibility());

  EXPECT_EQ("BackgroundFileIO", params_vector[1].name());
  EXPECT_EQ(ThreadPriority::BACKGROUND, params_vector[1].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[1].standby_thread_policy());
  EXPECT_EQ(2U, params_vector[1].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(52),
            params_vector[1].suggested_reclaim_time());
  EXPECT_EQ(base::SchedulerBackwardCompatibility::DISABLED,
            params_vector[1].backward_compatibility());

  EXPECT_EQ("Foreground", params_vector[2].name());
  EXPECT_EQ(ThreadPriority::NORMAL, params_vector[2].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[2].standby_thread_policy());
  EXPECT_EQ(4U, params_vector[2].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(62),
            params_vector[2].suggested_reclaim_time());
  EXPECT_EQ(base::SchedulerBackwardCompatibility::DISABLED,
            params_vector[2].backward_compatibility());

  EXPECT_EQ("ForegroundFileIO", params_vector[3].name());
  EXPECT_EQ(ThreadPriority::NORMAL, params_vector[3].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[3].standby_thread_policy());
  EXPECT_EQ(8U, params_vector[3].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(72),
            params_vector[3].suggested_reclaim_time());
  EXPECT_EQ(base::SchedulerBackwardCompatibility::INIT_COM_STA,
            params_vector[3].backward_compatibility());
}

TEST_F(TaskSchedulerUtilVariationsUtilTest, OrderingParams6) {
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
  EXPECT_EQ(base::SchedulerBackwardCompatibility::DISABLED,
            params_vector[0].backward_compatibility());

  EXPECT_EQ("BackgroundFileIO", params_vector[1].name());
  EXPECT_EQ(ThreadPriority::BACKGROUND, params_vector[1].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[1].standby_thread_policy());
  EXPECT_EQ(2U, params_vector[1].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(52),
            params_vector[1].suggested_reclaim_time());
  EXPECT_EQ(base::SchedulerBackwardCompatibility::DISABLED,
            params_vector[1].backward_compatibility());

  EXPECT_EQ("Foreground", params_vector[2].name());
  EXPECT_EQ(ThreadPriority::NORMAL, params_vector[2].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::LAZY,
            params_vector[2].standby_thread_policy());
  EXPECT_EQ(4U, params_vector[2].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(62),
            params_vector[2].suggested_reclaim_time());
  EXPECT_EQ(base::SchedulerBackwardCompatibility::DISABLED,
            params_vector[2].backward_compatibility());

  EXPECT_EQ("ForegroundFileIO", params_vector[3].name());
  EXPECT_EQ(ThreadPriority::NORMAL, params_vector[3].priority_hint());
  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[3].standby_thread_policy());
  EXPECT_EQ(8U, params_vector[3].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(72),
            params_vector[3].suggested_reclaim_time());
  EXPECT_EQ(base::SchedulerBackwardCompatibility::INIT_COM_STA,
            params_vector[3].backward_compatibility());
}

TEST_F(TaskSchedulerUtilVariationsUtilTest, NoData) {
  EXPECT_TRUE(GetWorkerPoolParams(GetImmutableWorkerPoolParams(),
                                  std::map<std::string, std::string>())
                  .empty());
}

TEST_F(TaskSchedulerUtilVariationsUtilTest, IncompleteParameters) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "1;1;1;0";
  variation_params["BackgroundFileIO"] = "2;2;1;0";
  variation_params["Foreground"] = "4;4;1;0";
  variation_params["ForegroundFileIO"] = "8;8;1;0";
  EXPECT_TRUE(
      GetWorkerPoolParams(GetImmutableWorkerPoolParams(), variation_params)
          .empty());
}

TEST_F(TaskSchedulerUtilVariationsUtilTest, InvalidParameters) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "a;b;c;d;e";
  variation_params["BackgroundFileIO"] = "a;b;c;d;e";
  variation_params["Foreground"] = "a;b;c;d;e";
  variation_params["ForegroundFileIO"] = "a;b;c;d;e";
  EXPECT_TRUE(
      GetWorkerPoolParams(GetImmutableWorkerPoolParams(), variation_params)
          .empty());
}

#if !defined(OS_IOS)
// Verify that AddVariationParamsToCommandLine() serializes BrowserScheduler
// variation params that start with the specified prefix to the command line and
// that GetVariationParamsFromCommandLine() correctly deserializes them.
TEST_F(TaskSchedulerUtilVariationsUtilTest, CommandLine) {
  std::map<std::string, std::string> in_variation_params;
  in_variation_params["PrefixFoo"] = "Foo";
  in_variation_params["PrefixBar"] = "Bar";
  in_variation_params["NoPrefix"] = "NoPrefix";
  ASSERT_TRUE(variations::AssociateVariationParams(
      kFieldTrialName, kFieldTrialTestGroup, in_variation_params));
  base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kFieldTrialTestGroup);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  AddVariationParamsToCommandLine("Prefix", &command_line);
  const std::map<std::string, std::string> out_variation_params =
      GetVariationParamsFromCommandLine(command_line);

  std::map<std::string, std::string> expected_out_variation_params;
  expected_out_variation_params["PrefixFoo"] = "Foo";
  expected_out_variation_params["PrefixBar"] = "Bar";
  EXPECT_EQ(expected_out_variation_params, out_variation_params);
}

// Verify that AddVariationParamsToCommandLine() doesn't add anything to the
// command line when a BrowserScheduler variation param key contains |. A key
// that contains | wouldn't be deserialized correctly by
// GetVariationParamsFromCommandLine().
TEST_F(TaskSchedulerUtilVariationsUtilTest,
       CommandLineSeparatorInVariationParamsKey) {
  std::map<std::string, std::string> in_variation_params;
  in_variation_params["PrefixFoo"] = "Foo";
  in_variation_params["PrefixKey|"] = "Value";
  ASSERT_TRUE(variations::AssociateVariationParams(
      kFieldTrialName, kFieldTrialTestGroup, in_variation_params));
  base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kFieldTrialTestGroup);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  AddVariationParamsToCommandLine("Prefix", &command_line);
  EXPECT_TRUE(
      command_line.GetSwitchValueASCII(kTaskSchedulerVariationParamsSwitch)
          .empty());
}

// Verify that AddVariationParamsToCommandLine() doesn't add anything to the
// command line when a BrowserScheduler variation param value contains |. A
// value that contains | wouldn't be deserialized correctly by
// GetVariationParamsFromCommandLine().
TEST_F(TaskSchedulerUtilVariationsUtilTest,
       CommandLineSeparatorInVariationParamsValue) {
  std::map<std::string, std::string> in_variation_params;
  in_variation_params["PrefixFoo"] = "Foo";
  in_variation_params["PrefixKey"] = "Value|";
  ASSERT_TRUE(variations::AssociateVariationParams(
      kFieldTrialName, kFieldTrialTestGroup, in_variation_params));
  base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kFieldTrialTestGroup);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  AddVariationParamsToCommandLine("Prefix", &command_line);
  EXPECT_TRUE(
      command_line.GetSwitchValueASCII(kTaskSchedulerVariationParamsSwitch)
          .empty());
}

// Verify that GetVariationParamsFromCommandLine() returns an empty map when the
// command line doesn't have a --task-scheduler-variation-params switch.
TEST_F(TaskSchedulerUtilVariationsUtilTest, CommandLineNoSwitch) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(GetVariationParamsFromCommandLine(command_line).empty());
}
#endif  // !defined(OS_IOS)

}  // namespace task_scheduler_util
