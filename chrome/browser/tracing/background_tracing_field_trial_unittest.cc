// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tracing/common/trace_startup.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class BackgroundTracingTest : public testing::Test {
 public:
  BackgroundTracingTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        ui_thread_(content::BrowserThread::UI,
                   base::ThreadTaskRunnerHandle::Get()) {}
  ~BackgroundTracingTest() override {}

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThread ui_thread_;
};

namespace {

const char kTestConfig[] = "test";
bool g_test_config_loaded = false;

void CheckConfig(std::string* config) {
  if (*config == kTestConfig)
    g_test_config_loaded = true;
}

}  // namespace

TEST_F(BackgroundTracingTest, SetupBackgroundTracingFieldTrial) {
  base::FieldTrialList field_trial_list(nullptr);
  const std::string kTrialName = "BackgroundTracing";
  const std::string kExperimentName = "SlowStart";
  base::AssociateFieldTrialParams(kTrialName, kExperimentName,
                                  {{"config", kTestConfig}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kExperimentName);

  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());

  // In case it is already set at previous test run.
  g_test_config_loaded = false;

  tracing::SetConfigTextFilterForTesting(&CheckConfig);

  tracing::SetupBackgroundTracingFieldTrial();
  EXPECT_TRUE(g_test_config_loaded);
}
