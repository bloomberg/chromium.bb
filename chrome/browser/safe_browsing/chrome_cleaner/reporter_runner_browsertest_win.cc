// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"

#include <initializer_list>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/component_updater/pref_names.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/variations/variations_params_manager.h"

namespace safe_browsing {

namespace {

constexpr char kSRTPromptGroup[] = "SRTGroup";

// Parameters for this test:
//  - bool in_browser_cleaner_ui_: indicates if InBrowserCleanerUI feature
//    is enabled;
//
//    We expect that the reporter's logic should remain unchanged even when the
//    InBrowserCleanerUI feature is enabled with one exception: the reporter is
//    not run daily because with the new feature enabled there is no concept of
//    a pending prompt. See the RunDaily and InBrowserUINoRunDaily tests.
//  - const char* old_seed_: The old "Seed" Finch parameter saved in prefs.
//  - const char* incoming_seed_: The new "Seed" Finch parameter.
class ReporterRunnerTest : public InProcessBrowserTest,
                           public SwReporterTestingDelegate,
                           public ::testing::WithParamInterface<
                               testing::tuple<bool, const char*, const char*>> {
 public:
  ReporterRunnerTest() {
    std::tie(in_browser_cleaner_ui_, old_seed_, incoming_seed_) = GetParam();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    variations::testing::VariationParamsManager::AppendVariationParams(
        kSRTPromptTrial, kSRTPromptGroup, {{"Seed", incoming_seed_}},
        command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SetSwReporterTestingDelegate(this);

    if (in_browser_cleaner_ui_)
      scoped_feature_list_.InitAndEnableFeature(kInBrowserCleanerUIFeature);
    else
      scoped_feature_list_.InitAndDisableFeature(kInBrowserCleanerUIFeature);
  }

  void SetUpOnMainThread() override {
    // During the test, use a task runner with a mock clock.
    saved_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    ASSERT_NE(mock_time_task_runner_, saved_task_runner_);
    base::MessageLoop::current()->SetTaskRunner(mock_time_task_runner_);

    // SetDateInLocalState calculates a time as Now() minus an offset. Move the
    // simulated clock ahead far enough that this calculation won't underflow.
    mock_time_task_runner_->FastForwardBy(
        base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns * 2));

    ClearLastTimeSentReport();

    chrome::FindLastActive()->profile()->GetPrefs()->SetString(
        prefs::kSwReporterPromptSeed, old_seed_);
  }

  void TearDownOnMainThread() override {
    // Restore the standard task runner to perform browser cleanup, which will
    // wait forever with the mock clock.
    base::MessageLoop::current()->SetTaskRunner(saved_task_runner_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    SetSwReporterTestingDelegate(nullptr);
  }

  // Records that the prompt was shown.
  void TriggerPrompt() override { prompt_trigger_called_ = true; }

  // Records that the reporter was launched with the parameters given in
  // |invocation|
  int LaunchReporter(const SwReporterInvocation& invocation) override {
    ++reporter_launch_count_;
    reporter_launch_parameters_.push_back(invocation);
    if (first_launch_callback_)
      std::move(first_launch_callback_).Run();
    return exit_code_to_report_;
  }

  // Returns the test's idea of the current time.
  base::Time Now() const override { return mock_time_task_runner_->Now(); }

  // Returns a task runner to use when launching the reporter (which is
  // normally a blocking action).
  base::TaskRunner* BlockingTaskRunner() const override {
    // Use the test's main task runner so that we don't need to pump another
    // message loop. Since the test calls LaunchReporter instead of actually
    // doing a blocking reporter launch, it doesn't matter that the task runner
    // doesn't have the MayBlock trait.
    return mock_time_task_runner_.get();
  }

  // Schedules a single reporter to run.
  void RunReporter(int exit_code_to_report,
                   const base::FilePath& exe_path = base::FilePath()) {
    exit_code_to_report_ = exit_code_to_report;
    auto invocation = SwReporterInvocation::FromFilePath(exe_path);
    invocation.supported_behaviours =
        SwReporterInvocation::BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS |
        SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT |
        SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS;

    SwReporterQueue invocations;
    invocations.push_back(invocation);
    RunSwReporters(invocations, base::Version("1.2.3"));
  }

  // Schedules a queue of reporters to run.
  void RunReporterQueue(int exit_code_to_report,
                        const SwReporterQueue& invocations) {
    exit_code_to_report_ = exit_code_to_report;
    RunSwReporters(invocations, base::Version("1.2.3"));
  }

  // Sets |path| in the local state to a date corresponding to |days| days ago.
  void SetDateInLocalState(const std::string& path, int days) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetInt64(
        path, (Now() - base::TimeDelta::FromDays(days)).ToInternalValue());
  }

  // Sets local state for last time the software reporter ran to |days| days
  // ago.
  void SetDaysSinceLastTriggered(int days) {
    SetDateInLocalState(prefs::kSwReporterLastTimeTriggered, days);
  }

  // Sets local state for last time the software reporter sent logs to |days|
  // days ago.
  void SetLastTimeSentReport(int days) {
    SetDateInLocalState(prefs::kSwReporterLastTimeSentReport, days);
  }

  // Clears local state for last time the software reporter sent logs. This
  // prevents potential false positives that could arise from state not
  // properly cleaned between successive tests.
  void ClearLastTimeSentReport() {
    PrefService* local_state = g_browser_process->local_state();
    local_state->ClearPref(prefs::kSwReporterLastTimeSentReport);
  }

  // Retrieves the timestamp of the last time the software reporter sent logs.
  int64_t GetLastTimeSentReport() {
    const PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state->HasPrefPath(prefs::kSwReporterLastTimeSentReport));
    return local_state->GetInt64(prefs::kSwReporterLastTimeSentReport);
  }

  void EnableSBExtendedReporting() {
    Browser* browser = chrome::FindLastActive();
    DCHECK(browser);
    Profile* profile = browser->profile();
    SetExtendedReportingPref(profile->GetPrefs(), true);
  }

  // Expects that the reporter has been scheduled to launch again in |days|
  // days.
  void ExpectToRunAgain(int days) {
    EXPECT_TRUE(mock_time_task_runner_->HasPendingTask());
    EXPECT_LT(base::TimeDelta::FromDays(days) - base::TimeDelta::FromHours(1),
              mock_time_task_runner_->NextPendingTaskDelay());
    EXPECT_GE(base::TimeDelta::FromDays(days),
              mock_time_task_runner_->NextPendingTaskDelay());
  }

  // Expects that after |days_until_launch| days, the reporter will be
  // launched |expected_launch_count| times, and TriggerPrompt will be
  // called if and only if |expect_prompt| is true.
  void ExpectReporterLaunches(int days_until_launch,
                              int expected_launch_count,
                              bool expect_prompt) {
    EXPECT_TRUE(mock_time_task_runner_->HasPendingTask());
    reporter_launch_count_ = 0;
    reporter_launch_parameters_.clear();
    prompt_trigger_called_ = false;

    mock_time_task_runner_->FastForwardBy(
        base::TimeDelta::FromDays(days_until_launch));

    EXPECT_EQ(expected_launch_count, reporter_launch_count_);
    EXPECT_EQ(expect_prompt, prompt_trigger_called_);
  }

  // Expects that after |days_until_launched| days, the reporter will be
  // launched once with each path in |expected_launch_paths|, and TriggerPrompt
  // will be called if and only if |expect_prompt| is true.
  void ExpectReporterLaunches(
      int days_until_launch,
      const std::vector<base::FilePath>& expected_launch_paths,
      bool expect_prompt) {
    ExpectReporterLaunches(days_until_launch, expected_launch_paths.size(),
                           expect_prompt);
    EXPECT_EQ(expected_launch_paths.size(), reporter_launch_parameters_.size());
    for (size_t i = 0; i < expected_launch_paths.size() &&
                       i < reporter_launch_parameters_.size();
         ++i) {
      EXPECT_EQ(expected_launch_paths[i],
                reporter_launch_parameters_[i].command_line.GetProgram());
    }
  }

  void ExpectLastTimeSentReportNotSet() {
    PrefService* local_state = g_browser_process->local_state();
    EXPECT_FALSE(
        local_state->HasPrefPath(prefs::kSwReporterLastTimeSentReport));
  }

  void ExpectLastReportSentInTheLastHour() {
    const PrefService* local_state = g_browser_process->local_state();
    const base::Time now = Now();
    const base::Time last_time_sent_logs = base::Time::FromInternalValue(
        local_state->GetInt64(prefs::kSwReporterLastTimeSentReport));

    // Checks if the last time sent logs is set as no more than one hour ago,
    // which should be enough time if the execution does not fail.
    EXPECT_LT(now - base::TimeDelta::FromHours(1), last_time_sent_logs);
    EXPECT_GE(now, last_time_sent_logs);
  }

  // Expects |invocation|'s command line to contain all the switches required
  // for reporter logging if and only if |expect_switches| is true.
  void ExpectLoggingSwitches(const SwReporterInvocation& invocation,
                             bool expect_switches) {
    static const std::set<std::string> logging_switches{
        chrome_cleaner::kExtendedSafeBrowsingEnabledSwitch,
        chrome_cleaner::kChromeVersionSwitch,
        chrome_cleaner::kChromeChannelSwitch};

    const base::CommandLine::SwitchMap& invocation_switches =
        invocation.command_line.GetSwitches();
    // Checks if switches that enable logging on the reporter are present on
    // the invocation if and only if logging is allowed.
    for (const std::string& logging_switch : logging_switches) {
      EXPECT_EQ(expect_switches, invocation_switches.find(logging_switch) !=
                                     invocation_switches.end());
    }
  }

  // A task runner with a mock clock.
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_ =
      new base::TestMockTimeTaskRunner();

  // The task runner that was in use before installing |mock_time_task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> saved_task_runner_;

  // Test parameters.
  bool in_browser_cleaner_ui_;
  std::string old_seed_;
  std::string incoming_seed_;

  bool prompt_trigger_called_ = false;
  int reporter_launch_count_ = 0;
  std::vector<SwReporterInvocation> reporter_launch_parameters_;
  int exit_code_to_report_ = kReporterNotLaunchedExitCode;

  // A callback to invoke when the first reporter of a queue is launched. This
  // can be used to perform actions in the middle of a queue of reporters which
  // all launch on the same mock clock tick.
  base::OnceClosure first_launch_callback_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReporterRunnerTest);
};

class ReporterRunnerPromptTest : public DialogBrowserTest {
 public:
  void ShowDialog(const std::string& name) override {
    if (name == "SRTErrorNoFile")
      DisplaySRTPromptForTesting(base::FilePath());
    else if (name == "SRTErrorFile")
      DisplaySRTPromptForTesting(
          base::FilePath().Append(FILE_PATH_LITERAL("c:\temp\testfile.txt")));
    else
      ADD_FAILURE() << "Unknown dialog type.";
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, NothingFound) {
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(0, 1, false);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, CleanupNeeded) {
  bool expect_prompt = incoming_seed_.empty() || (incoming_seed_ != old_seed_);

  RunReporter(chrome_cleaner::kSwReporterCleanupNeeded);
  ExpectReporterLaunches(0, 1, expect_prompt);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, RanRecently) {
  constexpr int kDaysLeft = 1;
  SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns - kDaysLeft);
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(0, 0, false);
  ExpectToRunAgain(kDaysLeft);
  ExpectReporterLaunches(kDaysLeft, 1, false);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

// Test is flaky. crbug.com/705608
IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, DISABLED_WaitForBrowser) {
  Profile* profile = browser()->profile();

  // Ensure that even though we're closing the last browser, we don't enter the
  // "shutting down" state, which will prevent the test from starting another
  // browser.
  ScopedKeepAlive test_keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                                  KeepAliveRestartOption::ENABLED);

  // Use the standard task runner for browser cleanup, which will wait forever
  // with the mock clock.
  base::MessageLoop::current()->SetTaskRunner(saved_task_runner_);
  CloseBrowserSynchronously(browser());
  base::MessageLoop::current()->SetTaskRunner(mock_time_task_runner_);
  ASSERT_EQ(0u, chrome::GetTotalBrowserCount());
  ASSERT_FALSE(chrome::FindLastActive());

  // Start the reporter while the browser is closed. The prompt should not open.
  RunReporter(chrome_cleaner::kSwReporterCleanupNeeded);
  ExpectReporterLaunches(0, 1, false);

  // Create a Browser object directly instead of using helper functions like
  // CreateBrowser, because they all wait on timed events but do not advance the
  // mock timer. The Browser constructor registers itself with the global
  // BrowserList, which is cleaned up when InProcessBrowserTest exits.
  new Browser(Browser::CreateParams(profile, /*user_gesture=*/false));
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  // Some browser startup tasks are scheduled to run in the first few minutes
  // after creation. Make sure they've all been processed so that the only
  // pending task in the queue is the next reporter check.
  mock_time_task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(10));

  // On opening the new browser, the prompt should be shown and the reporter
  // should be scheduled to run later.
  EXPECT_EQ(1, reporter_launch_count_);
  EXPECT_TRUE(prompt_trigger_called_);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, Failure) {
  RunReporter(kReporterNotLaunchedExitCode);
  ExpectReporterLaunches(0, 1, false);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, RunDaily) {
  // When the kInBrowserCleanerUIFeature feature is enabled, the reporter should
  // never run daily. Test that case separately.
  if (in_browser_cleaner_ui_)
    return;

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kSwReporterPendingPrompt, true);
  SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns - 1);
  ASSERT_GT(kDaysBetweenSuccessfulSwReporterRuns - 1,
            kDaysBetweenSwReporterRunsForPendingPrompt);
  RunReporter(chrome_cleaner::kSwReporterNothingFound);

  // Expect the reporter to run immediately, since a prompt is pending and it
  // has been more than kDaysBetweenSwReporterRunsForPendingPrompt days.
  ExpectReporterLaunches(0, 1, false);
  ExpectToRunAgain(kDaysBetweenSwReporterRunsForPendingPrompt);

  // Move the clock ahead kDaysBetweenSwReporterRunsForPendingPrompt days. The
  // expected run should trigger, but not cause the reporter to launch because
  // a prompt is no longer pending.
  local_state->SetBoolean(prefs::kSwReporterPendingPrompt, false);
  ExpectReporterLaunches(kDaysBetweenSwReporterRunsForPendingPrompt, 0, false);

  // Instead it should now run kDaysBetweenSuccessfulSwReporterRuns after the
  // first prompt (of which kDaysBetweenSwReporterRunsForPendingPrompt has
  // already passed.)
  int days_left = kDaysBetweenSuccessfulSwReporterRuns -
                  kDaysBetweenSwReporterRunsForPendingPrompt;
  ExpectToRunAgain(days_left);
  ExpectReporterLaunches(days_left, 1, false);
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, InBrowserUINoRunDaily) {
  // Ensure that the reporter always runs every
  // kDaysBetweenSuccessfulSwReporterRuns days when kInBrowserCleanerUIFeature
  // is enabled. The case when kInBrowserCleanerUIFeature is disabled is tested
  // separately.
  if (!in_browser_cleaner_ui_)
    return;

  // Users can have the pending prompt set to true when migrating to the new UI,
  // but it should be disregarded and the reporter should only be run every
  // kDaysBetweenSuccessfulSwReporterRuns days.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kSwReporterPendingPrompt, true);
  SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns - 1);
  ASSERT_GT(kDaysBetweenSuccessfulSwReporterRuns - 1,
            kDaysBetweenSwReporterRunsForPendingPrompt);
  RunReporter(chrome_cleaner::kSwReporterNothingFound);

  // Pending prompt is set, but we should not run the reporter since it hasn't
  // been kDaysBetweenSuccessfulSwReporterRuns days since the last run.
  ExpectReporterLaunches(0, 0, false);
  ExpectToRunAgain(1);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, ParameterChange) {
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
    SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns - kDaysLeft);
    RunReporter(chrome_cleaner::kSwReporterNothingFound, path1);
    ExpectReporterLaunches(0, {}, false);
  }

  // Schedule path2 just as we enter the next reporting period.
  // Now the reporter should launch, just once, using path2.
  {
    SCOPED_TRACE("Reporter runs now");
    RunReporter(chrome_cleaner::kSwReporterNothingFound, path2);
    // Schedule it twice; it should only actually run once.
    RunReporter(chrome_cleaner::kSwReporterNothingFound, path2);
    ExpectReporterLaunches(kDaysLeft, {path2}, false);
  }

  // Schedule path3 before any more time has passed.
  // The reporter should not launch.
  {
    SCOPED_TRACE("No more time passed");
    RunReporter(chrome_cleaner::kSwReporterNothingFound, path3);
    ExpectReporterLaunches(0, {}, false);
  }

  // Enter the next reporting period as path3 is still scheduled.
  // Now the reporter should launch again using path3. (Tests that the
  // parameters from the first launch aren't reused.)
  {
    SCOPED_TRACE("Previous run still scheduled");
    ExpectReporterLaunches(kDaysBetweenSuccessfulSwReporterRuns, {path3},
                           false);
  }

  // Schedule path3 again in the next reporting period.
  // The reporter should launch again using path3, since enough time has
  // passed, even though the parameters haven't changed.
  {
    SCOPED_TRACE("Run with same parameters");
    RunReporter(chrome_cleaner::kSwReporterNothingFound, path3);
    ExpectReporterLaunches(kDaysBetweenSuccessfulSwReporterRuns, {path3},
                           false);
  }

  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, MultipleLaunches) {
  const base::FilePath path1(L"path1");
  const base::FilePath path2(L"path2");
  const base::FilePath path3(L"path3");

  SwReporterQueue invocations;
  invocations.push_back(SwReporterInvocation::FromFilePath(path1));
  invocations.push_back(SwReporterInvocation::FromFilePath(path2));

  {
    SCOPED_TRACE("Launch 2 times");
    SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);
    ExpectReporterLaunches(0, {path1, path2}, false);
    ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
  }

  // Schedule a launch with 2 elements, then another with the same 2. It should
  // just run 2 times, not 4.
  {
    SCOPED_TRACE("Launch 2 times with retry");
    RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);
    RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);
    ExpectReporterLaunches(kDaysBetweenSuccessfulSwReporterRuns, {path1, path2},
                           false);
    ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
  }

  // Another launch with 2 elements is already scheduled. Add a third while the
  // queue is running.
  {
    SCOPED_TRACE("Add third launch while running");
    invocations.push_back(SwReporterInvocation::FromFilePath(path3));
    first_launch_callback_ = base::BindOnce(
        &ReporterRunnerTest::RunReporterQueue, base::Unretained(this),
        chrome_cleaner::kSwReporterNothingFound, invocations);

    // Only the first two elements should execute since the third was added
    // during the cycle.
    ExpectReporterLaunches(kDaysBetweenSuccessfulSwReporterRuns, {path1, path2},
                           false);
    ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);

    // Time passes... Now the 3-element queue should run.
    ExpectReporterLaunches(kDaysBetweenSuccessfulSwReporterRuns,
                           {path1, path2, path3}, false);
    ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
  }

  // Second launch should not occur after a failure.
  {
    SCOPED_TRACE("Launch multiple times with failure");
    RunReporterQueue(kReporterNotLaunchedExitCode, invocations);
    ExpectReporterLaunches(kDaysBetweenSuccessfulSwReporterRuns, {path1},
                           false);
    ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);

    // If we try again before the reporting period is up, it should not do
    // anything.
    ExpectReporterLaunches(0, {}, false);

    // After enough time has passed, should try the queue again.
    ExpectReporterLaunches(kDaysBetweenSuccessfulSwReporterRuns, {path1},
                           false);
    ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       ReporterLogging_NoSBExtendedReporting) {
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(0, 1, false);
  ExpectLoggingSwitches(reporter_launch_parameters_.front(), false);
  ExpectLastTimeSentReportNotSet();
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, ReporterLogging_EnabledFirstRun) {
  EnableSBExtendedReporting();
  // Note: don't set last time sent logs in the local state.
  // SBER is enabled and there is no record in the local state of the last time
  // logs have been sent, so we should send logs in this run.
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(0, 1, false);
  ExpectLoggingSwitches(reporter_launch_parameters_.front(), true);
  ExpectLastReportSentInTheLastHour();
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       ReporterLogging_EnabledNoRecentLogging) {
  // SBER is enabled and last time logs were sent was more than
  // |kDaysBetweenReporterLogsSent| day ago, so we should send logs in this run.
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent + 3);
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(0, 1, false);
  ExpectLoggingSwitches(reporter_launch_parameters_.front(), true);
  ExpectLastReportSentInTheLastHour();
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       ReporterLogging_EnabledRecentlyLogged) {
  // SBER is enabled, but logs have been sent less than
  // |kDaysBetweenReporterLogsSent| day ago, so we shouldn't send any logs in
  // this run.
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent - 1);
  int64_t last_time_sent_logs = GetLastTimeSentReport();
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(0, 1, false);
  ExpectLoggingSwitches(reporter_launch_parameters_.front(), false);
  EXPECT_EQ(last_time_sent_logs, GetLastTimeSentReport());
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, ReporterLogging_MultipleLaunches) {
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent + 3);

  const base::FilePath path1(L"path1");
  const base::FilePath path2(L"path2");
  SwReporterQueue invocations;
  for (const auto& path : {path1, path2}) {
    auto invocation = SwReporterInvocation::FromFilePath(path);
    invocation.supported_behaviours =
        SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS;
    invocations.push_back(invocation);
  }
  RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);

  // SBER is enabled and last time logs were sent was more than
  // |kDaysBetweenReporterLogsSent| day ago, so we should send logs in this run.
  {
    SCOPED_TRACE("first launch");
    first_launch_callback_ =
        base::BindOnce(&ReporterRunnerTest::ExpectLastReportSentInTheLastHour,
                       base::Unretained(this));
    ExpectReporterLaunches(0, {path1, path2}, false);
    ExpectLoggingSwitches(reporter_launch_parameters_[0], true);
  }

  // Logs should also be sent for the next run, even though LastTimeSentReport
  // is now recent, because the run is part of the same set of invocations.
  {
    SCOPED_TRACE("second launch");
    ExpectLoggingSwitches(reporter_launch_parameters_[1], true);
    ExpectLastReportSentInTheLastHour();
  }
  ExpectToRunAgain(kDaysBetweenSuccessfulSwReporterRuns);
}

INSTANTIATE_TEST_CASE_P(
    WithInBrowserCleanerUIParam,
    ReporterRunnerTest,
    ::testing::Combine(
        ::testing::Bool(),                          // in_browser_cleaner_ui_
        ::testing::Values("", "Seed1"),             // old_seed_
        ::testing::Values("", "Seed1", "Seed2")));  // incoming_seed

// This provide tests which allows explicit invocation of the SRT Prompt
// useful for checking dialog layout or any other interactive functionality
// tests. See docs/testing/test_browser_dialog.md for description of the
// testing framework.
IN_PROC_BROWSER_TEST_F(ReporterRunnerPromptTest, InvokeDialog_SRTErrorNoFile) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ReporterRunnerPromptTest, InvokeDialog_SRTErrorFile) {
  RunDialog();
}

}  // namespace safe_browsing
