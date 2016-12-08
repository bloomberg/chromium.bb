// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/variations/browser_variations_util.h"

#include <map>
#include <vector>

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_scheduler_util {
namespace variations {

namespace {

using StandbyThreadPolicy =
    base::SchedulerWorkerPoolParams::StandbyThreadPolicy;

constexpr char kFieldTrialName[] = "BrowserScheduler";
constexpr char kFieldTrialTestGroup[] = "Test";

class TaskSchedulerUtilBrowserVariationsUtilTest : public ::testing::Test {
 public:
  TaskSchedulerUtilBrowserVariationsUtilTest() : field_trial_list_(nullptr) {}

  ~TaskSchedulerUtilBrowserVariationsUtilTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    ::variations::testing::ClearAllVariationIDs();
    ::variations::testing::ClearAllVariationParams();
  }

 private:
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerUtilBrowserVariationsUtilTest);
};

}  // namespace

TEST_F(TaskSchedulerUtilBrowserVariationsUtilTest, OrderingParams5) {
  std::map<std::string, std::string> params;
  params["Background"] = "1;1;1;0;42";
  params["BackgroundFileIO"] = "2;2;1;0;52";
  params["Foreground"] = "4;4;1;0;62";
  params["ForegroundFileIO"] = "8;8;1;0;72";
  ASSERT_TRUE(::variations::AssociateVariationParams(kFieldTrialName,
                                                     kFieldTrialTestGroup,
                                                     params));

  base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kFieldTrialTestGroup);
  auto params_vector = GetBrowserSchedulerWorkerPoolParamsFromVariations();
  ASSERT_EQ(4U, params_vector.size());

  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[0].standby_thread_policy());
  EXPECT_EQ(1U, params_vector[0].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(42),
            params_vector[0].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[1].standby_thread_policy());
  EXPECT_EQ(2U, params_vector[1].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(52),
            params_vector[1].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[2].standby_thread_policy());
  EXPECT_EQ(4U, params_vector[2].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(62),
            params_vector[2].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[3].standby_thread_policy());
  EXPECT_EQ(8U, params_vector[3].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(72),
            params_vector[3].suggested_reclaim_time());
}

TEST_F(TaskSchedulerUtilBrowserVariationsUtilTest, OrderingParams6) {
  std::map<std::string, std::string> params;
  params["Background"] = "1;1;1;0;42;lazy";
  params["BackgroundFileIO"] = "2;2;1;0;52;one";
  params["Foreground"] = "4;4;1;0;62;lazy";
  params["ForegroundFileIO"] = "8;8;1;0;72;one";
  ASSERT_TRUE(::variations::AssociateVariationParams(kFieldTrialName,
                                                     kFieldTrialTestGroup,
                                                     params));

  base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kFieldTrialTestGroup);
  auto params_vector = GetBrowserSchedulerWorkerPoolParamsFromVariations();
  ASSERT_EQ(4U, params_vector.size());

  EXPECT_EQ(StandbyThreadPolicy::LAZY,
            params_vector[0].standby_thread_policy());
  EXPECT_EQ(1U, params_vector[0].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(42),
            params_vector[0].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[1].standby_thread_policy());
  EXPECT_EQ(2U, params_vector[1].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(52),
            params_vector[1].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::LAZY,
            params_vector[2].standby_thread_policy());
  EXPECT_EQ(4U, params_vector[2].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(62),
            params_vector[2].suggested_reclaim_time());

  EXPECT_EQ(StandbyThreadPolicy::ONE, params_vector[3].standby_thread_policy());
  EXPECT_EQ(8U, params_vector[3].max_threads());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(72),
            params_vector[3].suggested_reclaim_time());
}

TEST_F(TaskSchedulerUtilBrowserVariationsUtilTest, NoData) {
  auto params_vector = GetBrowserSchedulerWorkerPoolParamsFromVariations();
  EXPECT_TRUE(params_vector.empty());
}

TEST_F(TaskSchedulerUtilBrowserVariationsUtilTest, IncompleteParameters) {
  std::map<std::string, std::string> params;
  params["Background"] = "1;1;1;0";
  params["BackgroundFileIO"] = "2;2;1;0";
  params["Foreground"] = "4;4;1;0";
  params["ForegroundFileIO"] = "8;8;1;0";
  base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kFieldTrialTestGroup);
  ASSERT_TRUE(::variations::AssociateVariationParams(kFieldTrialName,
                                                     kFieldTrialTestGroup,
                                                     params));
  auto params_vector = GetBrowserSchedulerWorkerPoolParamsFromVariations();
  EXPECT_TRUE(params_vector.empty());
}

TEST_F(TaskSchedulerUtilBrowserVariationsUtilTest, InvalidParameters) {
  std::map<std::string, std::string> params;
  params["Background"] = "a;b;c;d;e";
  params["BackgroundFileIO"] = "a;b;c;d;e";
  params["Foreground"] = "a;b;c;d;e";
  params["ForegroundFileIO"] = "a;b;c;d;e";
  base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kFieldTrialTestGroup);
  ASSERT_TRUE(::variations::AssociateVariationParams(kFieldTrialName,
                                                     kFieldTrialTestGroup,
                                                     params));
  auto params_vector = GetBrowserSchedulerWorkerPoolParamsFromVariations();
  EXPECT_TRUE(params_vector.empty());
}

}  // namespace variations
}  // namespace task_scheduler_util
