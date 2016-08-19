// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_fetcher_win.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
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

class SRTFetcherTest : public InProcessBrowserTest,
                       public SwReporterTestingDelegate {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    task_runner_ = new base::TestSimpleTaskRunner;

    SetSwReporterTestingDelegate(this);
  }

  void TearDownInProcessBrowserTestFixture() override {
    SetSwReporterTestingDelegate(nullptr);
  }

  void RunReporter(const base::FilePath& exe_path = base::FilePath()) {
    RunSwReporter(SwReporterInvocation::FromFilePath(exe_path),
                  base::Version("1.2.3"), task_runner_, task_runner_);
  }

  void TriggerPrompt(Browser* browser, const std::string& version) override {
    prompt_trigger_called_ = true;
  }

  int LaunchReporter(const SwReporterInvocation& invocation) override {
    ++reporter_launch_count_;
    reporter_launch_parameters_ = invocation;
    return exit_code_to_report_;
  }

  void NotifyLaunchReady() override { launch_ready_notified_ = true; }

  void NotifyReporterDone() override { reporter_done_notified_ = true; }

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

  void TestReporterLaunchCycle(int expected_launch_count,
                               const base::FilePath& expected_launch_path) {
    // This test has an unfortunate amount of knowledge of the internals of
    // ReporterRunner, because it needs to pump the right message loops at the
    // right time so that all its internal messages are delivered. This
    // function might need to be updated if the internals change.
    //
    // The basic sequence is:

    // 1. TryToRun kicks the whole thing off. If the reporter should not be
    // launched now (eg. DaysSinceLastReport is too low) it posts a call to
    // itself again. (In a regular task runner this will be scheduled with a
    // delay, but the test task runner ignores delays so TryToRun will be
    // called again on the next call to RunPendingTasks.)
    //
    // 2. When it is time to run a reporter, TryToRun calls NotifyLaunchReady
    // and then posts a call to LaunchAndWait.
    //
    // 3. When the reporter returns, a call to ReporterDone is posted on the UI
    // thread.
    //
    // 4. ReporterDone calls NotifyReporterDone and then posts another call to
    // TryToRun, which starts the whole process over for the next run.
    //
    // Each call to RunPendingTasks only handles messages already on the queue.
    // It doesn't handle messages posted by those messages. So, we need to call
    // it in a loop to make sure we're past all pending TryToRun calls before
    // LaunchAndWaitForExit will be called.
    //
    // Once a call to LaunchAndWaitForExit has been posted, TryToRun won't be
    // called again until we pump the UI message loop in order to run
    // ReporterDone.

    ASSERT_TRUE(task_runner_->HasPendingTask());
    ASSERT_FALSE(reporter_done_notified_);

    // Clear out any pending TryToRun calls. Use a bounded loop so that if the
    // reporter will never be called, we'll eventually continue. (If
    // LaunchAndWaitForExit was pending but TryToRun wasn't, this will call
    // it.)
    const int max_expected_launch_count = reporter_launch_count_ + 1;
    for (int i = 0; !launch_ready_notified_ && i < 100; ++i)
      task_runner_->RunPendingTasks();
    ASSERT_GE(max_expected_launch_count, reporter_launch_count_);

    // Reset launch_ready_notified_ so that we can tell if TryToRun gets called
    // again unexpectedly.
    launch_ready_notified_ = false;

    // This will trigger LaunchAndWaitForExit if it's on the queue and hasn't
    // been called already. If it was called already, this will do nothing.
    // (There definitely isn't anything queued up yet after
    // LaunchAndWaitForExit because ReporterDone wasn't called yet.)
    task_runner_->RunPendingTasks();
    ASSERT_GE(max_expected_launch_count, reporter_launch_count_);
    ASSERT_FALSE(launch_ready_notified_);
    ASSERT_FALSE(reporter_done_notified_);

    // At this point LaunchAndWaitForExit has definitely been called if it's
    // going to be called at all. (If not, TryToRun will have been scheduled
    // again.)
    EXPECT_EQ(expected_launch_count, reporter_launch_count_);
    EXPECT_EQ(expected_launch_path,
              reporter_launch_parameters_.command_line.GetProgram());

    // Pump the UI message loop to process the ReporterDone call (which will
    // schedule the next TryToRun.) If LaunchAndWaitForExit wasn't called, this
    // does nothing.
    base::RunLoop().RunUntilIdle();

    // At this point another call to TryToRun should be scheduled, whether or
    // not LaunchAndWaitForExit was called.
    ASSERT_TRUE(task_runner_->HasPendingTask());

    // Make sure the flags are false for the next launch cycle test.
    ASSERT_FALSE(launch_ready_notified_);
    reporter_done_notified_ = false;
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  bool prompt_trigger_called_ = false;
  int reporter_launch_count_ = 0;
  SwReporterInvocation reporter_launch_parameters_;
  int exit_code_to_report_ = kReporterFailureExitCode;
  bool launch_ready_notified_ = false;
  bool reporter_done_notified_ = false;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, NothingFound) {
  exit_code_to_report_ = kSwReporterNothingFound;
  RunReporter();
  task_runner_->RunPendingTasks();
  EXPECT_EQ(1, reporter_launch_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(prompt_trigger_called_);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, CleanupNeeded) {
  exit_code_to_report_ = kSwReporterCleanupNeeded;
  RunReporter();

  task_runner_->RunPendingTasks();
  EXPECT_EQ(1, reporter_launch_count_);
  // The reply task from the task posted to run the reporter is run on a
  // specific thread, as opposed to a specific task runner, and that thread is
  // the current message loop's thread.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(prompt_trigger_called_);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, RanRecently) {
  constexpr int kDaysLeft = 1;
  SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns - kDaysLeft);
  RunReporter();

  // Here we can't run until idle since the ReporterRunner will re-post
  // infinitely.
  task_runner_->RunPendingTasks();
  EXPECT_EQ(0, reporter_launch_count_);

  ExpectToRunAgain(kDaysLeft);
  task_runner_->ClearPendingTasks();
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, WaitForBrowser) {
  Profile* profile = browser()->profile();
  CloseAllBrowsers();

  exit_code_to_report_ = kSwReporterCleanupNeeded;
  RunReporter();

  task_runner_->RunPendingTasks();
  EXPECT_EQ(1, reporter_launch_count_);

  CreateBrowser(profile);
  EXPECT_TRUE(prompt_trigger_called_);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, Failure) {
  exit_code_to_report_ = kReporterFailureExitCode;
  RunReporter();

  task_runner_->RunPendingTasks();
  EXPECT_EQ(1, reporter_launch_count_);

  base::RunLoop().RunUntilIdle();
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
  EXPECT_EQ(1, reporter_launch_count_);
  reporter_launch_count_ = 0;
  base::RunLoop().RunUntilIdle();
  ExpectToRunAgain(kDaysBetweenSwReporterRunsForPendingPrompt);

  local_state->SetBoolean(prefs::kSwReporterPendingPrompt, false);
  task_runner_->RunPendingTasks();
  EXPECT_EQ(0, reporter_launch_count_);
  base::RunLoop().RunUntilIdle();
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, ParameterChange) {
  exit_code_to_report_ = kSwReporterNothingFound;

  // If the reporter is run several times with different parameters, it should
  // only be launched once, with the last parameter set.
  const base::FilePath path1(L"path1");
  const base::FilePath path2(L"path2");
  const base::FilePath path3(L"path3");

  // Schedule path1 with a day left in the reporting period.
  // The reporter should not launch.
  constexpr int kDaysLeft = 1;
  {
    SCOPED_TRACE("N days left until next reporter run");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns - kDaysLeft);
    RunReporter(path1);
    TestReporterLaunchCycle(0, base::FilePath());
  }

  // Schedule path2 just as we enter the next reporting period.
  // Now the reporter should launch, just once, using path2.
  {
    SCOPED_TRACE("Reporter runs now");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporter(path2);
    // Schedule it twice; it should only actually run once.
    RunReporter(path2);
    TestReporterLaunchCycle(1, path2);
  }

  // Schedule path3 before any more time has passed.
  // The reporter should not launch.
  {
    SCOPED_TRACE("No more time passed");
    RunReporter(path3);
    TestReporterLaunchCycle(1, path2);
  }

  // Enter the next reporting period as path3 is still scheduled.
  // Now the reporter should launch again using path3. (Tests that the
  // parameters from the first launch aren't reused.)
  {
    SCOPED_TRACE("Previous run still scheduled");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    TestReporterLaunchCycle(2, path3);
  }

  // Schedule path3 again in the next reporting period.
  // The reporter should launch again using path3, since enough time has
  // passed, even though the parameters haven't changed.
  {
    SCOPED_TRACE("Run with same parameters");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporter(path3);
    TestReporterLaunchCycle(3, path3);
  }
}

}  // namespace safe_browsing
