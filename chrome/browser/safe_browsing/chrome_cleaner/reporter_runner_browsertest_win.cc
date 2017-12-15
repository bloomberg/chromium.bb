// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"

#include <initializer_list>
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
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;

constexpr char kSRTPromptGroup[] = "SRTGroup";

class Waiter {
 public:
  Waiter() = default;
  ~Waiter() = default;

  void Wait() {
    while (!Done())
      base::RunLoop().RunUntilIdle();
  }

  bool Done() {
    base::AutoLock lock(lock_);
    return done_;
  }

  void Signal() {
    base::AutoLock lock(lock_);
    done_ = true;
  }

 private:
  bool done_ = false;
  base::Lock lock_;
};

// Parameters for this test:
//  - bool user_initiated_runs_enabled_: identifies if feature
//        UserInitiatedChromeCleanups is enabled.
//  - SwReporterInvocationType invocation_type_: identifies the type of
//        invocation being tested.
//  - const char* old_seed_: The old "Seed" Finch parameter saved in prefs.
//  - const char* incoming_seed_: The new "Seed" Finch parameter.
using ReporterRunnerTestParams =
    std::tuple<bool, SwReporterInvocationType, const char*, const char*>;

class ReporterRunnerTest
    : public InProcessBrowserTest,
      public SwReporterTestingDelegate,
      public ::testing::WithParamInterface<ReporterRunnerTestParams> {
 public:
  ReporterRunnerTest() {
    std::tie(user_initiated_runs_enabled_, invocation_type_, old_seed_,
             incoming_seed_) = GetParam();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    variations::testing::VariationParamsManager::AppendVariationParams(
        kSRTPromptTrial, kSRTPromptGroup, {{"Seed", incoming_seed_}},
        command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SetSwReporterTestingDelegate(this);
  }

  void SetUpOnMainThread() override {
    if (user_initiated_runs_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          kUserInitiatedChromeCleanupsFeature);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kUserInitiatedChromeCleanupsFeature);
    }

    // SetDateInLocalState calculates a time as Now() minus an offset. Move the
    // simulated clock ahead far enough that this calculation won't underflow.
    FastForwardBy(
        base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns * 2));

    ClearLastTimeSentReport();

    chrome::FindLastActive()->profile()->GetPrefs()->SetString(
        prefs::kSwReporterPromptSeed, old_seed_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    SetSwReporterTestingDelegate(nullptr);
  }

  // Records that the reporter was launched with the parameters given in
  // |invocation|.
  int LaunchReporter(const SwReporterInvocation& invocation) override {
    ++reporter_launch_count_;
    reporter_launch_parameters_.push_back(invocation);
    if (first_launch_callback_)
      std::move(first_launch_callback_).Run();
    return exit_code_to_report_;
  }

  // Returns the test's idea of the current time.
  base::Time Now() const override { return now_; }

  ChromeCleanerController* GetCleanerController() override {
    return &mock_chrome_cleaner_controller_;
  }

  void CreateChromeCleanerDialogController() override {
    dialog_controller_created_ = true;
  }

  void FastForwardBy(base::TimeDelta delta) { now_ += delta; }

  // Returns a task runner to use when launching the reporter (which is
  // normally a blocking action).
  base::TaskRunner* BlockingTaskRunner() const override {
    // Use the test's main task runner so that we don't need to pump another
    // message loop. Since the test calls LaunchReporter instead of actually
    // doing a blocking reporter launch, it doesn't matter that the task runner
    // doesn't have the MayBlock trait.
    return base::ThreadTaskRunnerHandle::Get().get();
  }

  void ResetReporterRuns(int exit_code_to_report) {
    exit_code_to_report_ = exit_code_to_report;
    reporter_launch_count_ = 0;
    reporter_launch_parameters_.clear();
    dialog_controller_created_ = false;
  }

  SwReporterInvocation CreateInvocation(const base::FilePath& exe_path) {
    return SwReporterInvocation(base::CommandLine(exe_path))
        .WithSupportedBehaviours(
            SwReporterInvocation::BEHAVIOURS_ENABLED_BY_DEFAULT);
  }

  SwReporterInvocationSequence CreateInvocationSequence(
      const SwReporterInvocationSequence::Queue& container) {
    SwReporterInvocationSequence sequence(base::Version("1.2.3"));
    sequence.mutable_container() = container;
    return sequence;
  }

  // Schedules a single reporter to run.
  void RunSingleReporterAndWait(int exit_code_to_report,
                                SwReporterInvocationResult expected_result) {
    RunReporterQueueAndWait(exit_code_to_report, expected_result,
                            SwReporterInvocationSequence::Queue(
                                {CreateInvocation(base::FilePath())}));
  }

  // Schedules a queue of reporters to run.
  void RunReporterQueueAndWait(
      int exit_code_to_report,
      SwReporterInvocationResult expected_result,
      const SwReporterInvocationSequence::Queue& container) {
    ResetReporterRuns(exit_code_to_report);

    ON_CALL(mock_chrome_cleaner_controller_, state())
        .WillByDefault(Return(ChromeCleanerController::State::kIdle));

    if (expected_result != SwReporterInvocationResult::kNotScheduled) {
      EXPECT_CALL(mock_chrome_cleaner_controller_, OnReporterSequenceStarted())
          .Times(1);
    }

    Waiter on_sequence_done;
    EXPECT_CALL(mock_chrome_cleaner_controller_,
                OnReporterSequenceDone(Eq(expected_result)))
        .WillOnce(InvokeWithoutArgs(&on_sequence_done, &Waiter::Signal));
    auto invocations = CreateInvocationSequence(container);
    RunSwReportersForTesting(invocation_type_, std::move(invocations));
    on_sequence_done.Wait();
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

  // Expects that the reporter has been launched |expected_launch_count| times,
  // and TriggerPrompt will be called if and only if |expect_prompt| is true.
  void ExpectNumReporterLaunches(int expected_launch_count,
                                 bool expect_prompt) {
    EXPECT_EQ(expected_launch_count, reporter_launch_count_);
    EXPECT_EQ(expect_prompt, dialog_controller_created_);
  }

  // Expects that the reporter has been launched once with each path in
  // |expected_launch_paths|, and TriggerPrompt will be called if and only
  // if |expect_prompt| is true.
  void ExpectReporterLaunches(
      const std::vector<base::FilePath>& expected_launch_paths,
      bool expect_prompt) {
    ExpectNumReporterLaunches(expected_launch_paths.size(), expect_prompt);
    EXPECT_EQ(expected_launch_paths.size(), reporter_launch_parameters_.size());
    for (size_t i = 0; i < expected_launch_paths.size() &&
                       i < reporter_launch_parameters_.size();
         ++i) {
      EXPECT_EQ(expected_launch_paths[i],
                reporter_launch_parameters_[i].command_line().GetProgram());
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
        invocation.command_line().GetSwitches();
    // Checks if switches that enable logging on the reporter are present on
    // the invocation if and only if logging is allowed.
    for (const std::string& logging_switch : logging_switches) {
      EXPECT_EQ(expect_switches, invocation_switches.find(logging_switch) !=
                                     invocation_switches.end());
    }
  }

  void ExpectReporterLoggingHappenedInTheLastHour(
      const SwReporterInvocation& invocation) {
    ExpectLoggingSwitches(invocation, true);
    ExpectLastReportSentInTheLastHour();
  }

  void FutureExpectReporterLoggingHappenedInTheLastHour(
      base::OnceCallback<const SwReporterInvocation&()> get_invocation) {
    ExpectReporterLoggingHappenedInTheLastHour(std::move(get_invocation).Run());
  }

  void ExpectNoReporterLoggingWithLastTimeSentReportNotSet(
      const SwReporterInvocation& invocation) {
    ExpectLoggingSwitches(invocation, false);
    ExpectLastTimeSentReportNotSet();
  }

  void ExpectNoReporterLoggingWithLastTimeSentReportSet(
      const SwReporterInvocation& invocation,
      int64_t last_time_sent_logs) {
    ExpectLoggingSwitches(invocation, false);
    EXPECT_EQ(last_time_sent_logs, GetLastTimeSentReport());
  }

  void FutureExpectNoReporterLoggingWithLastTimeSentReportSet(
      base::OnceCallback<const SwReporterInvocation&()> get_invocation,
      int64_t last_time_sent_logs) {
    ExpectNoReporterLoggingWithLastTimeSentReportSet(
        std::move(get_invocation).Run(), last_time_sent_logs);
  }

  bool LogsEnabledByUser() {
    return invocation_type_ ==
           SwReporterInvocationType::kUserInitiatedWithLogsAllowed;
  }

  bool LogsDisabledByUser() {
    return invocation_type_ ==
           SwReporterInvocationType::kUserInitiatedWithLogsDisallowed;
  }

  void ExpectResultOnSequenceDone(SwReporterInvocationResult expected_result,
                                  base::OnceClosure closure,
                                  SwReporterInvocationResult result) {
    EXPECT_EQ(expected_result, result);
    std::move(closure).Run();
  }

  OnReporterSequenceDone ExpectResultOnSequenceDoneCallback(
      SwReporterInvocationResult expected_result,
      base::OnceClosure closure) {
    return base::BindOnce(&ReporterRunnerTest::ExpectResultOnSequenceDone,
                          base::Unretained(this), expected_result,
                          base::Passed(&closure));
  }

  bool PromptDialogShouldBeShown(SwReporterInvocationType invocation_type) {
    return !IsUserInitiated(invocation_type) &&
           (incoming_seed_.empty() || (incoming_seed_ != old_seed_));
  }

  // Tests that a sequence of type |invocation_type1|, once scheduled, will not
  // allow a sequence of type |invocation_type2| be scheduled.
  void TestSequenceNotScheduled(SwReporterInvocationType invocation_type1,
                                SwReporterInvocationType invocation_type2) {
    const base::FilePath path1(L"path1");
    const base::FilePath path2(L"path2");

    ResetReporterRuns(chrome_cleaner::kSwReporterNothingFound);

    ON_CALL(mock_chrome_cleaner_controller_, state())
        .WillByDefault(Return(ChromeCleanerController::State::kIdle));
    EXPECT_CALL(mock_chrome_cleaner_controller_, OnReporterSequenceStarted())
        .Times(1);

    // Launch two sequences that will be run in the following order:
    //  - The first sequence (of type |invocation_type1|) starts and waits to
    //    proceed;
    //  - The second sequence (of type |invocation_type2|) is scheduled and this
    //    test is blocked until all sequences complete;
    //  - The second sequence can't be scheduled and its invocation will not
    //    run, because the first sequence is already running; it will then be
    //    deleted and will release the first sequence that is waiting to
    //    proceed;
    //  - The first sequence, once unblocked, will finish and unblock this test;
    //  - At the end, check if only the invocation in the first sequence (path1)
    //    was run.
    Waiter first_sequence_done;
    Waiter second_sequence_done;

    EXPECT_CALL(
        mock_chrome_cleaner_controller_,
        OnReporterSequenceDone(Eq(SwReporterInvocationResult::kNothingFound)))
        .WillOnce(
            DoAll(InvokeWithoutArgs(&second_sequence_done, &Waiter::Wait),
                  InvokeWithoutArgs(&first_sequence_done, &Waiter::Signal)));

    SwReporterInvocationSequence::Queue invocations1({CreateInvocation(path1)});
    RunSwReportersForTesting(invocation_type1,
                             CreateInvocationSequence(invocations1));

    EXPECT_CALL(
        mock_chrome_cleaner_controller_,
        OnReporterSequenceDone(Eq(SwReporterInvocationResult::kNotScheduled)))
        .WillOnce(InvokeWithoutArgs(&second_sequence_done, &Waiter::Signal));

    SwReporterInvocationSequence::Queue invocations2({CreateInvocation(path2)});
    RunSwReportersForTesting(invocation_type2,
                             CreateInvocationSequence(invocations2));

    first_sequence_done.Wait();

    ExpectReporterLaunches({path1}, /*expect_prompt=*/false);
  }

  base::Time now_;

  // Test parameters.
  bool user_initiated_runs_enabled_;
  SwReporterInvocationType invocation_type_;
  std::string old_seed_;
  std::string incoming_seed_;

  bool dialog_controller_created_ = false;
  int reporter_launch_count_ = 0;
  std::vector<SwReporterInvocation> reporter_launch_parameters_;
  int exit_code_to_report_ = kReporterNotLaunchedExitCode;

  // Using NiceMock to suppress warnings about uninteresting calls to state().
  ::testing::NiceMock<MockChromeCleanerController>
      mock_chrome_cleaner_controller_;

  // A callback to invoke when the first reporter of a queue is launched. This
  // can be used to perform actions in the middle of a queue of reporters which
  // all launch on the same mock clock tick.
  base::OnceClosure first_launch_callback_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReporterRunnerTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, NothingFound) {
  RunSingleReporterAndWait(chrome_cleaner::kSwReporterNothingFound,
                           SwReporterInvocationResult::kNothingFound);
  ExpectNumReporterLaunches(/*expected_launch_count=*/1,
                            /*expect_prompt=*/false);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, CleanupNeeded) {
  bool cleanup_offered =
      IsUserInitiated(invocation_type_) ||
      (incoming_seed_.empty() || (incoming_seed_ != old_seed_));

  SwReporterInvocation scan_invocation = CreateInvocation(base::FilePath());
  if (cleanup_offered) {
    EXPECT_CALL(mock_chrome_cleaner_controller_, Scan(_))
        .WillOnce(SaveArg<0>(&scan_invocation));
  }

  RunSingleReporterAndWait(
      chrome_cleaner::kSwReporterCleanupNeeded,
      cleanup_offered ? SwReporterInvocationResult::kCleanupToBeOffered
                      : SwReporterInvocationResult::kCleanupNotOffered);
  ExpectNumReporterLaunches(
      /*expected_launch_count=*/1,
      /*expect_prompt=*/PromptDialogShouldBeShown(invocation_type_));

  if (cleanup_offered) {
    EXPECT_EQ(invocation_type_ ==
                  SwReporterInvocationType::kUserInitiatedWithLogsAllowed,
              scan_invocation.cleaner_logs_upload_enabled());
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, RanRecently) {
  constexpr int kDaysLeft = 1;
  SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns - kDaysLeft);
  if (IsUserInitiated(invocation_type_)) {
    RunSingleReporterAndWait(chrome_cleaner::kSwReporterNothingFound,
                             SwReporterInvocationResult::kNothingFound);
    ExpectNumReporterLaunches(/*expected_launch_count=*/1,
                              /*expect_prompt=*/false);
  } else {
    RunSingleReporterAndWait(chrome_cleaner::kSwReporterNothingFound,
                             SwReporterInvocationResult::kNotScheduled);
    ExpectNumReporterLaunches(/*expected_launch_count=*/0,
                              /*expect_prompt=*/false);
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, FailureToLaunch) {
  RunSingleReporterAndWait(kReporterNotLaunchedExitCode,
                           SwReporterInvocationResult::kProcessFailedToLaunch);
  ExpectNumReporterLaunches(/*expected_launch_count=*/1,
                            /*expect_prompt=*/false);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, GeneralFailure) {
  constexpr int kFailureExitCode = 1;
  RunSingleReporterAndWait(kFailureExitCode,
                           SwReporterInvocationResult::kGeneralFailure);
  ExpectNumReporterLaunches(/*expected_launch_count=*/1,
                            /*expect_prompt=*/false);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, MultipleLaunches) {
  constexpr int kAFewDays = 3;

  const base::FilePath path1(L"path1");
  const base::FilePath path2(L"path2");

  SwReporterInvocationSequence::Queue invocations(
      {CreateInvocation(path1), CreateInvocation(path2)});

  {
    SCOPED_TRACE("Launch 2 times");
    SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporterQueueAndWait(chrome_cleaner::kSwReporterNothingFound,
                            SwReporterInvocationResult::kNothingFound,
                            invocations);
    ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);
  }

  // Schedule a launch with 2 invocations, then another with the same 2.
  {
    SCOPED_TRACE("Launch 2 times with retry");
    FastForwardBy(
        base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns));
    RunReporterQueueAndWait(chrome_cleaner::kSwReporterNothingFound,
                            SwReporterInvocationResult::kNothingFound,
                            invocations);
    ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);

    if (IsUserInitiated(invocation_type_)) {
      // If user initiated, the second invocation sequence should run again.
      RunReporterQueueAndWait(chrome_cleaner::kSwReporterNothingFound,
                              SwReporterInvocationResult::kNothingFound,
                              invocations);
      ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);
    } else {
      // For periodic runs, the second invocation sequence shouldn't run again,
      // since the first sequence has just finished.
      RunReporterQueueAndWait(chrome_cleaner::kSwReporterNothingFound,
                              SwReporterInvocationResult::kNotScheduled,
                              invocations);
      ExpectReporterLaunches({}, /*expect_prompt=*/false);
    }
  }

  {
    SCOPED_TRACE("Attempt to launch after a few days");
    FastForwardBy(base::TimeDelta::FromDays(kAFewDays));
    if (IsUserInitiated(invocation_type_)) {
      RunReporterQueueAndWait(chrome_cleaner::kSwReporterNothingFound,
                              SwReporterInvocationResult::kNothingFound,
                              invocations);
      ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);
    } else {
      RunReporterQueueAndWait(chrome_cleaner::kSwReporterNothingFound,
                              SwReporterInvocationResult::kNotScheduled,
                              invocations);
      ExpectReporterLaunches({}, /*expect_prompt=*/false);
    }

    FastForwardBy(base::TimeDelta::FromDays(
        kDaysBetweenSuccessfulSwReporterRuns - kAFewDays));
    RunReporterQueueAndWait(chrome_cleaner::kSwReporterNothingFound,
                            SwReporterInvocationResult::kNothingFound,
                            invocations);
    ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);
  }

  // Second launch should not occur after a failure.
  {
    SCOPED_TRACE("Launch multiple times with failure");
    FastForwardBy(
        base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns));
    RunReporterQueueAndWait(kReporterNotLaunchedExitCode,
                            SwReporterInvocationResult::kProcessFailedToLaunch,
                            invocations);
    ExpectReporterLaunches({path1}, /*expect_prompt=*/false);
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       ReporterLogging_NoSBExtendedReporting) {
  RunSingleReporterAndWait(chrome_cleaner::kSwReporterNothingFound,
                           SwReporterInvocationResult::kNothingFound);
  ExpectNumReporterLaunches(/*expected_launch_count=*/1,
                            /*expect_prompt=*/false);
  if (LogsEnabledByUser()) {
    ExpectReporterLoggingHappenedInTheLastHour(
        reporter_launch_parameters_.front());
  } else {
    ExpectNoReporterLoggingWithLastTimeSentReportNotSet(
        reporter_launch_parameters_.front());
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, ReporterLogging_EnabledFirstRun) {
  EnableSBExtendedReporting();
  // Note: don't set last time sent logs in the local state.
  // SBER is enabled and there is no record in the local state of the last time
  // logs have been sent, so we should send logs in this run.
  RunSingleReporterAndWait(chrome_cleaner::kSwReporterNothingFound,
                           SwReporterInvocationResult::kNothingFound);
  ExpectNumReporterLaunches(/*expected_launch_count=*/1,
                            /*expect_prompt=*/false);
  if (LogsDisabledByUser()) {
    ExpectNoReporterLoggingWithLastTimeSentReportNotSet(
        reporter_launch_parameters_.front());
  } else {
    ExpectReporterLoggingHappenedInTheLastHour(
        reporter_launch_parameters_.front());
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       ReporterLogging_EnabledNoRecentLogging) {
  // SBER is enabled and last time logs were sent was more than
  // |kDaysBetweenReporterLogsSent| day ago, so we should send logs in this run.
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent + 3);
  int64_t last_time_sent_logs = GetLastTimeSentReport();
  RunSingleReporterAndWait(chrome_cleaner::kSwReporterNothingFound,
                           SwReporterInvocationResult::kNothingFound);
  ExpectNumReporterLaunches(/*expected_launch_count=*/1,
                            /*expect_prompt=*/false);
  if (LogsDisabledByUser()) {
    ExpectNoReporterLoggingWithLastTimeSentReportSet(
        reporter_launch_parameters_.front(), last_time_sent_logs);
  } else {
    ExpectReporterLoggingHappenedInTheLastHour(
        reporter_launch_parameters_.front());
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       ReporterLogging_EnabledRecentlyLogged) {
  // SBER is enabled, but logs have been sent less than
  // |kDaysBetweenReporterLogsSent| day ago, so we shouldn't send any logs in
  // this run.
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent - 1);
  int64_t last_time_sent_logs = GetLastTimeSentReport();
  RunSingleReporterAndWait(chrome_cleaner::kSwReporterNothingFound,
                           SwReporterInvocationResult::kNothingFound);
  ExpectNumReporterLaunches(/*expected_launch_count=*/1,
                            /*expect_prompt=*/false);
  if (LogsEnabledByUser()) {
    ExpectReporterLoggingHappenedInTheLastHour(
        reporter_launch_parameters_.front());
  } else {
    ExpectNoReporterLoggingWithLastTimeSentReportSet(
        reporter_launch_parameters_.front(), last_time_sent_logs);
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, ReporterLogging_MultipleLaunches) {
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent + 3);
  int64_t last_time_sent_logs = GetLastTimeSentReport();

  const base::FilePath path1(L"path1");
  const base::FilePath path2(L"path2");

  SwReporterInvocationSequence::Queue invocations(
      {CreateInvocation(path1), CreateInvocation(path2)});

  auto get_first_invocation = base::BindOnce(
      [](const std::vector<SwReporterInvocation>* params)
          -> const SwReporterInvocation& { return params->front(); },
      &reporter_launch_parameters_);
  if (LogsDisabledByUser()) {
    first_launch_callback_ = base::BindOnce(
        &ReporterRunnerTest::
            FutureExpectNoReporterLoggingWithLastTimeSentReportSet,
        base::Unretained(this), base::Passed(&get_first_invocation),
        last_time_sent_logs);
  } else {
    first_launch_callback_ = base::BindOnce(
        &ReporterRunnerTest::FutureExpectReporterLoggingHappenedInTheLastHour,
        base::Unretained(this), base::Passed(&get_first_invocation));
  }

  RunReporterQueueAndWait(chrome_cleaner::kSwReporterNothingFound,
                          SwReporterInvocationResult::kNothingFound,
                          invocations);

  // SBER is enabled and last time logs were sent was more than
  // |kDaysBetweenReporterLogsSent| day ago, so we should send logs in this run.
  {
    SCOPED_TRACE("first launch");
    ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);
  }

  // Logs should also be sent for the next run, even though LastTimeSentReport
  // is now recent, because the run is part of the same set of invocations.
  {
    SCOPED_TRACE("second launch");
    if (LogsDisabledByUser()) {
      ExpectNoReporterLoggingWithLastTimeSentReportSet(
          reporter_launch_parameters_[1], last_time_sent_logs);
    } else {
      ExpectReporterLoggingHappenedInTheLastHour(
          reporter_launch_parameters_[1]);
    }
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       PeriodicRunsNotScheduledIfAlreadyRunning) {
  TestSequenceNotScheduled(invocation_type_,
                           SwReporterInvocationType::kPeriodicRun);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       UserInitiatedRunsNotScheduledIfAlreadyRunning) {
  // No need to test for periodic runs, since the corresponding test cases are
  // already exercised by PeriodicRunsNotScheduledIfAlreadyRunning.
  if (!IsUserInitiated(invocation_type_))
    return;

  TestSequenceNotScheduled(
      invocation_type_,
      SwReporterInvocationType::kUserInitiatedWithLogsDisallowed);
}

// Make the invocation type test parameter printable.
std::ostream& operator<<(std::ostream& out,
                         SwReporterInvocationType invocation_type) {
  switch (invocation_type) {
    case SwReporterInvocationType::kUnspecified:
      return out << "Unspecified";
    case SwReporterInvocationType::kPeriodicRun:
      return out << "PeriodicRun";
    case SwReporterInvocationType::kUserInitiatedWithLogsDisallowed:
      return out << "UserInitiatedWithLogsDisallowed";
    case SwReporterInvocationType::kUserInitiatedWithLogsAllowed:
      return out << "UserInitiatedWithLogsAllowed";
    default:
      NOTREACHED();
      return out << "UnknownSwReporterInvocationType";
  }
}

// ::testing::PrintToStringParamName does not format tuples as a valid test
// name, so this functor can be used to get each element in the tuple
// explicitly and format them using the above operator<< overrides.
struct ReporterRunTestParamsToString {
  std::string operator()(
      const ::testing::TestParamInfo<ReporterRunnerTestParams>& info) const {
    std::ostringstream param_name;
    param_name << std::get<0>(info.param) << "_";
    param_name << std::get<1>(info.param) << "_";
    const std::string old_seed(std::get<2>(info.param));
    param_name << (old_seed.empty() ? "EmptySeed" : old_seed) << "_";
    const std::string incoming_seed(std::get<3>(info.param));
    param_name << (incoming_seed.empty() ? "EmptySeed" : incoming_seed);
    return param_name.str();
  }
};

// Tests for kUserInitiatedChromeCleanupsFeature disabled (only periodic runs
// are possible).
INSTANTIATE_TEST_CASE_P(
    UserInitiatedRunsDisabled,
    ReporterRunnerTest,
    ::testing::Combine(
        ::testing::Values(false),  // user_initiated_runs_enabled_
        ::testing::Values(SwReporterInvocationType::kPeriodicRun),
        ::testing::Values("", "Seed1"),            // old_seed_
        ::testing::Values("", "Seed1", "Seed2")),  // incoming_seed_
    ReporterRunTestParamsToString());

// Tests for kUserInitiatedChromeCleanupsFeature enabled (all invocation types
// are allowed).
INSTANTIATE_TEST_CASE_P(
    UserInitiatedRunsEnabled,
    ReporterRunnerTest,
    ::testing::Combine(
        ::testing::Values(true),  // user_initiated_runs_enabled_
        ::testing::Values(
            SwReporterInvocationType::kPeriodicRun,
            SwReporterInvocationType::kUserInitiatedWithLogsDisallowed,
            SwReporterInvocationType::kUserInitiatedWithLogsAllowed),
        ::testing::Values("", "Seed1"),            // old_seed_
        ::testing::Values("", "Seed1", "Seed2")),  // incoming_seed_
    ReporterRunTestParamsToString());

}  // namespace safe_browsing
