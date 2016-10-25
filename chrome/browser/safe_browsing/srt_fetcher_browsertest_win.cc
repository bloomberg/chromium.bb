// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_fetcher_win.h"

#include <iterator>
#include <memory>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/srt_client_info_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"

namespace safe_browsing {

namespace {

const char* const kExpectedSwitches[] = {kExtendedSafeBrowsingEnabledSwitch,
                                         kChromeVersionSwitch,
                                         kChromeChannelSwitch};

class SRTFetcherTest : public InProcessBrowserTest,
                       public SwReporterTestingDelegate {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    task_runner_ = new base::TestSimpleTaskRunner;

    SetSwReporterTestingDelegate(this);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ClearLastTimeSentReport();
  }

  void TearDownInProcessBrowserTestFixture() override {
    SetSwReporterTestingDelegate(nullptr);
  }

  void RunReporter(const base::FilePath& exe_path = base::FilePath()) {
    auto invocation = SwReporterInvocation::FromFilePath(exe_path);
    invocation.supported_behaviours =
        SwReporterInvocation::BEHAVIOUR_LOG_TO_RAPPOR |
        SwReporterInvocation::BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS |
        SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT |
        SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS;

    SwReporterQueue invocations;
    invocations.push(invocation);
    RunSwReporters(invocations, base::Version("1.2.3"), task_runner_,
                   task_runner_);
  }

  void RunReporterQueue(const SwReporterQueue& invocations) {
    RunSwReporters(invocations, base::Version("1.2.3"), task_runner_,
                   task_runner_);
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

  // Sets |path| in the local state to a date corresponding to |days| days ago.
  void SetDateInLocalState(const std::string& path, int days) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK_NE(local_state, nullptr);
    local_state->SetInt64(path,
                          (base::Time::Now() - base::TimeDelta::FromDays(days))
                              .ToInternalValue());
  }

  void SetDaysSinceLastReport(int days) {
    SetDateInLocalState(prefs::kSwReporterLastTimeTriggered, days);
  }

  void ExpectToRunAgain(int days) {
    ASSERT_TRUE(task_runner_->HasPendingTask());
    EXPECT_LE(task_runner_->NextPendingTaskDelay(),
              base::TimeDelta::FromDays(days));
    EXPECT_GT(task_runner_->NextPendingTaskDelay(),
              base::TimeDelta::FromDays(days) - base::TimeDelta::FromHours(1));
  }

  // Clears local state for last time the software reporter sent logs to |days|
  // days ago. This prevents potential false positives that could arise from
  // state not properly cleaned between successive tests.
  void ClearLastTimeSentReport() {
    DCHECK_NE(g_browser_process, nullptr);
    PrefService* local_state = g_browser_process->local_state();
    DCHECK_NE(local_state, nullptr);
    local_state->ClearPref(prefs::kSwReporterLastTimeSentReport);
  }

  // Sets local state for last time the software reporter sent logs to |days|
  // days ago.
  void SetLastTimeSentReport(int days) {
    SetDateInLocalState(prefs::kSwReporterLastTimeSentReport, days);
  }

  int64_t GetLastTimeSentReport() {
    const PrefService* local_state = g_browser_process->local_state();
    DCHECK_NE(local_state, nullptr);
    DCHECK(local_state->HasPrefPath(prefs::kSwReporterLastTimeSentReport));
    return local_state->GetInt64(prefs::kSwReporterLastTimeSentReport);
  }

  void ExpectLastTimeSentReportNotSet() {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK_NE(local_state, nullptr);
    EXPECT_FALSE(
        local_state->HasPrefPath(prefs::kSwReporterLastTimeSentReport));
  }

  void ExpectLastReportSentInTheLastHour() {
    const PrefService* local_state = g_browser_process->local_state();
    DCHECK_NE(local_state, nullptr);
    const base::Time now = base::Time::Now();
    const base::Time last_time_sent_logs = base::Time::FromInternalValue(
        local_state->GetInt64(prefs::kSwReporterLastTimeSentReport));

    // Checks if the last time sent logs is set as no more than one hour ago,
    // which should be enough time if the execution does not fail.
    EXPECT_LT(now - base::TimeDelta::FromHours(1), last_time_sent_logs);
    EXPECT_LT(last_time_sent_logs, now);
  }

  // Run through the steps needed to launch the reporter, as many times as
  // needed to launch all the reporters given in |expected_launch_paths|. Test
  // that each of those launches succeeded. But do not test that ONLY those
  // launches succeeded.
  //
  // After this, if more launches are expected you can call
  // |TestPartialLaunchCycle| again with another list of paths, to test that
  // the launch cycle will continue with those paths.
  //
  // To test that a list of paths are launched AND NO OTHERS, use
  // |TestReporterLaunchCycle|.
  void TestPartialLaunchCycle(
      const std::vector<base::FilePath>& expected_launch_paths) {
    // This test has an unfortunate amount of knowledge of the internals of
    // ReporterRunner, because it needs to pump the right message loops at the
    // right time so that all its internal messages are delivered. This
    // function might need to be updated if the internals change.
    //
    // The basic sequence is:
    //
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

    reporter_launch_count_ = 0;
    reporter_launch_parameters_ = SwReporterInvocation();

    int current_launch_count = reporter_launch_count_;
    for (const auto& expected_launch_path : expected_launch_paths) {
      // If RunReporter was called with no pending messages, and it was already
      // time to launch the reporter, then |launch_ready_notified_| will
      // already be true. Otherwise there will be a TryToRun message pending,
      // which must be processed first.
      if (!launch_ready_notified_) {
        task_runner_->RunPendingTasks();
        // Since we're expecting a launch here, we expect it to schedule
        // LaunchAndWaitForExit. So NOW |launch_ready_notified_| should be
        // true.
        ASSERT_TRUE(task_runner_->HasPendingTask());
      }
      ASSERT_TRUE(launch_ready_notified_);
      ASSERT_EQ(current_launch_count, reporter_launch_count_);

      // Reset |launch_ready_notified_| so that we can tell if TryToRun gets
      // called again unexpectedly.
      launch_ready_notified_ = false;

      // Call the pending LaunchAndWaitForExit.
      task_runner_->RunPendingTasks();
      ASSERT_FALSE(launch_ready_notified_);
      ASSERT_FALSE(reporter_done_notified_);

      // At this point LaunchAndWaitForExit has definitely been called if
      // it's going to be called at all. (If not, TryToRun will have been
      // scheduled again.)
      EXPECT_EQ(current_launch_count + 1, reporter_launch_count_);
      EXPECT_EQ(expected_launch_path,
                reporter_launch_parameters_.command_line.GetProgram());

      // Pump the UI message loop to process the ReporterDone call (which
      // will schedule the next TryToRun.) If LaunchAndWaitForExit wasn't
      // called, this does nothing.
      base::RunLoop().RunUntilIdle();

      // At this point there are three things that could have happened:
      //
      // 1. LaunchAndWaitForExit was not called. There should be a TryToRun
      // scheduled.
      //
      // 2. ReporterDone was called and there was nothing left in the queue
      // of SwReporterInvocation's. There should be a TryToRun scheduled.
      //
      // 3. ReporterDone was called and there were more
      // SwReporterInvocation's in the queue to run immediately. There should
      // be a LaunchAndWaitForExit scheduled.
      //
      // So in all cases there should be a pending task, and if we are expecting
      // more launches in this loop, |launch_ready_notified_| will already be
      // true.
      ASSERT_TRUE(task_runner_->HasPendingTask());

      // The test task runner does not actually advance the clock. Pretend that
      // one day has passed. (Otherwise, when we launch the last
      // SwReporterInvocation in the queue, the next call to TryToRun will
      // start a whole new launch cycle.)
      SetDaysSinceLastReport(1);

      reporter_done_notified_ = false;
      current_launch_count = reporter_launch_count_;
    }
  }

  // Run through the steps needed to launch the reporter, as many times as
  // needed to launch all the reporters given in |expected_launch_paths|. Test
  // that each of those launches succeeded. Then, run through the steps needed
  // to launch the reporter again, to test that the launch cycle is complete
  // (no more reporters will be launched).
  void TestReporterLaunchCycle(
      const std::vector<base::FilePath>& expected_launch_paths) {
    TestPartialLaunchCycle(expected_launch_paths);

    // Now that all expected launches have been tested, run the cycle once more
    // to make sure no more launches happen.
    ASSERT_TRUE(task_runner_->HasPendingTask());
    ASSERT_FALSE(reporter_done_notified_);
    ASSERT_FALSE(launch_ready_notified_);

    int current_launch_count = reporter_launch_count_;

    // Call the pending TryToRun.
    task_runner_->RunPendingTasks();

    // We expect that this scheduled another TryToRun. If it scheduled
    // LaunchAndWaitForExit an unexpected launch is about to happen.
    ASSERT_TRUE(task_runner_->HasPendingTask());
    ASSERT_FALSE(launch_ready_notified_);
    ASSERT_FALSE(reporter_done_notified_);
    ASSERT_EQ(current_launch_count, reporter_launch_count_);
  }

  // Expects |reporter_launch_parameters_| to contain exactly the command line
  // switches specified in |expected_switches|.
  void ExpectLoggingSwitches(const std::set<std::string>& expected_switches) {
    const base::CommandLine::SwitchMap& invocation_switches =
        reporter_launch_parameters_.command_line.GetSwitches();
    EXPECT_EQ(expected_switches.size(), invocation_switches.size());
    // Checks if all expected switches are in the invocation switches. It's not
    // necessary to check if all invocation switches are expected, since we
    // checked if both sets should have the same size.
    for (const std::string& expected_switch : expected_switches) {
      EXPECT_NE(invocation_switches.find(expected_switch),
                invocation_switches.end());
    }
  }

  void EnableSBExtendedReporting() {
    Browser* browser = chrome::FindLastActive();
    ASSERT_NE(browser, nullptr);
    Profile* profile = browser->profile();
    ASSERT_NE(profile, nullptr);
    SetExtendedReportingPref(profile->GetPrefs(), true);
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  bool prompt_trigger_called_ = false;
  int reporter_launch_count_ = 0;
  SwReporterInvocation reporter_launch_parameters_;
  int exit_code_to_report_ = kReporterFailureExitCode;

  // This will be set to true when a call to |LaunchAndWaitForExit| is next in
  // the task queue.
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
    TestReporterLaunchCycle({});
  }

  // Schedule path2 just as we enter the next reporting period.
  // Now the reporter should launch, just once, using path2.
  {
    SCOPED_TRACE("Reporter runs now");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporter(path2);
    // Schedule it twice; it should only actually run once.
    RunReporter(path2);
    TestReporterLaunchCycle({path2});
  }

  // Schedule path3 before any more time has passed.
  // The reporter should not launch.
  {
    SCOPED_TRACE("No more time passed");
    SetDaysSinceLastReport(0);
    RunReporter(path3);
    TestReporterLaunchCycle({});
  }

  // Enter the next reporting period as path3 is still scheduled.
  // Now the reporter should launch again using path3. (Tests that the
  // parameters from the first launch aren't reused.)
  {
    SCOPED_TRACE("Previous run still scheduled");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    TestReporterLaunchCycle({path3});
  }

  // Schedule path3 again in the next reporting period.
  // The reporter should launch again using path3, since enough time has
  // passed, even though the parameters haven't changed.
  {
    SCOPED_TRACE("Run with same parameters");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporter(path3);
    TestReporterLaunchCycle({path3});
  }
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, MultipleLaunches) {
  exit_code_to_report_ = kSwReporterNothingFound;

  const base::FilePath path1(L"path1");
  const base::FilePath path2(L"path2");
  const base::FilePath path3(L"path3");

  SwReporterQueue invocations;
  invocations.push(SwReporterInvocation::FromFilePath(path1));
  invocations.push(SwReporterInvocation::FromFilePath(path2));

  {
    SCOPED_TRACE("Launch 2 times");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporterQueue(invocations);
    TestReporterLaunchCycle({path1, path2});
  }

  // Schedule a launch with 2 elements, then another with the same 2. It should
  // just run 2 times, not 4.
  {
    SCOPED_TRACE("Launch 2 times with retry");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporterQueue(invocations);
    RunReporterQueue(invocations);
    TestReporterLaunchCycle({path1, path2});
  }

  // Schedule a launch with 2 elements, then add a third while the queue is
  // running.
  {
    SCOPED_TRACE("Add third launch while running");
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporterQueue(invocations);

    // Only test the cycle once, to process the first element in queue.
    TestPartialLaunchCycle({path1});

    invocations.push(SwReporterInvocation::FromFilePath(path3));
    RunReporterQueue(invocations);

    // There is still a 2nd element on the queue - that should execute, and
    // nothing more.
    TestReporterLaunchCycle({path2});

    // Time passes... Now the 3-element queue should run.
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    TestReporterLaunchCycle({path1, path2, path3});
  }

  // Second launch should not occur after a failure.
  {
    SCOPED_TRACE("Launch multiple times with failure");
    exit_code_to_report_ = kReporterFailureExitCode;
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporterQueue(invocations);
    TestReporterLaunchCycle({path1});

    // If we try again before the reporting period is up, it should not do
    // anything.
    TestReporterLaunchCycle({});

    // After enough time has passed, should try the queue again.
    SetDaysSinceLastReport(kDaysBetweenSuccessfulSwReporterRuns);
    TestReporterLaunchCycle({path1});
  }
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, ReporterLogging_FeatureDisabled) {
  exit_code_to_report_ = kSwReporterNothingFound;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kSwReporterExtendedSafeBrowsingFeature);
  RunReporter();
  TestReporterLaunchCycle({base::FilePath()});
  ExpectLoggingSwitches({/*expect no switches*/});
  ExpectLastTimeSentReportNotSet();
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, ReporterLogging_NoSBExtendedReporting) {
  exit_code_to_report_ = kSwReporterNothingFound;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kSwReporterExtendedSafeBrowsingFeature);
  RunReporter();
  TestReporterLaunchCycle({base::FilePath()});
  ExpectLoggingSwitches({/*expect no switches*/});
  ExpectLastTimeSentReportNotSet();
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, ReporterLogging_EnabledFirstRun) {
  exit_code_to_report_ = kSwReporterNothingFound;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kSwReporterExtendedSafeBrowsingFeature);
  EnableSBExtendedReporting();
  // Note: don't set last time sent logs in the local state.
  // SBER is enabled and there is no record in the local state of the last time
  // logs have been sent, so we should send logs in this run.
  RunReporter();
  TestReporterLaunchCycle({base::FilePath()});
  ExpectLoggingSwitches(std::set<std::string>(std::begin(kExpectedSwitches),
                                              std::end(kExpectedSwitches)));
  ExpectLastReportSentInTheLastHour();
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, ReporterLogging_EnabledNoRecentLogging) {
  exit_code_to_report_ = kSwReporterNothingFound;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kSwReporterExtendedSafeBrowsingFeature);
  // SBER is enabled and last time logs were sent was more than
  // |kDaysBetweenReporterLogsSent| day ago, so we should send logs in this run.
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent + 3);
  RunReporter();
  TestReporterLaunchCycle({base::FilePath()});
  ExpectLoggingSwitches(std::set<std::string>(std::begin(kExpectedSwitches),
                                              std::end(kExpectedSwitches)));
  ExpectLastReportSentInTheLastHour();
}

IN_PROC_BROWSER_TEST_F(SRTFetcherTest, ReporterLogging_EnabledRecentlyLogged) {
  exit_code_to_report_ = kSwReporterNothingFound;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kSwReporterExtendedSafeBrowsingFeature);
  // SBER is enabled, but logs have been sent less than
  // |kDaysBetweenReporterLogsSent| day ago, so we shouldn't send any logs in
  // this run.
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent - 1);
  int64_t last_time_sent_logs = GetLastTimeSentReport();
  RunReporter();
  TestReporterLaunchCycle({base::FilePath()});
  ExpectLoggingSwitches(std::set<std::string>{/*expect no switches*/});
  EXPECT_EQ(last_time_sent_logs, GetLastTimeSentReport());
}

}  // namespace safe_browsing
