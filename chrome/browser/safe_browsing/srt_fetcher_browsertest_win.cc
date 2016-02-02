// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_fetcher_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"

namespace safe_browsing {

namespace {

class SRTFetcherTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    task_runner_ = new base::TestSimpleTaskRunner;

    SetReporterLauncherForTesting(base::Bind(
        &SRTFetcherTest::ReporterLauncherForTesting, base::Unretained(this)));
    SetPromptTriggerForTesting(base::Bind(
        &SRTFetcherTest::PromptTriggerForTesting, base::Unretained(this)));
  }

  void TearDownInProcessBrowserTestFixture() override {
    SetReporterLauncherForTesting(ReporterLauncher());
    SetPromptTriggerForTesting(PromptTrigger());
  }

  void RunReporter() {
    RunSwReporter(base::FilePath(), "bla", task_runner_, task_runner_);
  }

  void PromptTriggerForTesting(Browser* browser, const std::string& version) {
    prompt_trigger_called_ = true;
  }

  int ReporterLauncherForTesting(const base::FilePath& exe_path,
                       const std::string& version) {
    reporter_launched_ = true;
    return exit_code_to_report_;
  }

  void SetDaysSinceLastReport(int days) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetInt64(prefs::kSwReporterLastTimeTriggered,
                          (base::Time::Now() - base::TimeDelta::FromDays(days))
                              .ToInternalValue());
  }

  void ExpectToRunAgain(int days) {
    ASSERT_TRUE(task_runner_->HasPendingTask());
    EXPECT_LE(task_runner_->NextPendingTaskDelay(),
              base::TimeDelta::FromDays(days));
    EXPECT_GT(task_runner_->NextPendingTaskDelay(),
              base::TimeDelta::FromDays(days) - base::TimeDelta::FromHours(1));
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  bool prompt_trigger_called_ = false;
  bool reporter_launched_ = false;
  int exit_code_to_report_ = kReporterFailureExitCode;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, NothingFound) {
  exit_code_to_report_ = kSwReporterNothingFound;
  RunReporter();
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(reporter_launched_);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(prompt_trigger_called_);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, CleanupNeeded) {
  exit_code_to_report_ = kSwReporterCleanupNeeded;
  RunReporter();

  task_runner_->RunPendingTasks();
  EXPECT_TRUE(reporter_launched_);
  // The reply task from the task posted to run the reporter is run on a
  // specific thread, as opposed to a specific task runner, and that thread is
  // the current message loop's thread.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(prompt_trigger_called_);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, RanRecently) {
  static const int kDaysLeft = 1;
  SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns - kDaysLeft);
  RunReporter();

  // Here we can't run until idle since the ReporterRunner will re-post
  // infinitely.
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(reporter_launched_);

  ExpectToRunAgain(kDaysLeft);
  task_runner_->ClearPendingTasks();
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, WaitForBrowser) {
  Profile* profile = browser()->profile();
  CloseAllBrowsers();

  exit_code_to_report_ = kSwReporterCleanupNeeded;
  RunReporter();

  task_runner_->RunPendingTasks();
  EXPECT_TRUE(reporter_launched_);

  CreateBrowser(profile);
  EXPECT_TRUE(prompt_trigger_called_);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, Failure) {
  exit_code_to_report_ = kReporterFailureExitCode;
  RunReporter();

  task_runner_->RunPendingTasks();
  EXPECT_TRUE(reporter_launched_);

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(prompt_trigger_called_);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, RunDaily) {
  exit_code_to_report_ = kSwReporterNothingFound;
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kSwReporterPendingPrompt, true);
  SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns - 1);
  DCHECK_GT(kDaysBetweenSuccessfulSwReporterRuns - 1,
            kDaysBetweenSwReporterRunsForPendingPrompt);
  RunReporter();

  task_runner_->RunPendingTasks();
  EXPECT_TRUE(reporter_launched_);
  reporter_launched_ = false;
  base::MessageLoop::current()->RunUntilIdle();
  ExpectToRunAgain(kDaysBetweenSwReporterRunsForPendingPrompt);

  local_state->SetBoolean(prefs::kSwReporterPendingPrompt, false);
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(reporter_launched_);
  base::MessageLoop::current()->RunUntilIdle();
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

}  // namespace safe_browsing
