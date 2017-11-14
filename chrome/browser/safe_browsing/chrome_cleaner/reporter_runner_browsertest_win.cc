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
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
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

namespace safe_browsing {

namespace {

constexpr char kSRTPromptGroup[] = "SRTGroup";

class Waiter {
 public:
  Waiter() = default;
  ~Waiter() = default;

  void Wait() {
    while (!Done())
      base::RunLoop().RunUntilIdle();
  }

  base::OnceClosure SignalClosure() {
    return base::BindOnce(&Waiter::Signal, base::Unretained(this));
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
//  - const char* old_seed_: The old "Seed" Finch parameter saved in prefs.
//  - const char* incoming_seed_: The new "Seed" Finch parameter.
class ReporterRunnerTest : public InProcessBrowserTest,
                           public SwReporterTestingDelegate,
                           public ::testing::WithParamInterface<
                               testing::tuple<const char*, const char*>> {
 public:
  ReporterRunnerTest() { std::tie(old_seed_, incoming_seed_) = GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    variations::testing::VariationParamsManager::AppendVariationParams(
        kSRTPromptTrial, kSRTPromptGroup, {{"Seed", incoming_seed_}},
        command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SetSwReporterTestingDelegate(this);
  }

  void SetUpOnMainThread() override {
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

  // Records that the prompt was shown.
  void TriggerPrompt() override { prompt_trigger_called_ = true; }

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
    prompt_trigger_called_ = false;
  }

  // Schedules a single reporter to run and invokes |closure| when done.
  void RunReporterWithCallback(int exit_code_to_report,
                               const base::FilePath& exe_path,
                               base::OnceClosure closure) {
    auto invocation = SwReporterInvocation::FromFilePath(exe_path);
    invocation.supported_behaviours =
        SwReporterInvocation::BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS |
        SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT |
        SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS;
    RunSwReportersWithCallback(SwReporterQueue({invocation}),
                               base::Version("1.2.3"), std::move(closure));
  }

  // Schedules a single reporter to run.
  void RunReporter(int exit_code_to_report,
                   const base::FilePath& exe_path = base::FilePath()) {
    ResetReporterRuns(exit_code_to_report);
    Waiter sequence_done;
    RunReporterWithCallback(exit_code_to_report, exe_path,
                            sequence_done.SignalClosure());
    sequence_done.Wait();
  }

  // Schedules a queue of reporters to run.
  void RunReporterQueue(int exit_code_to_report,
                        const SwReporterQueue& invocations) {
    ResetReporterRuns(exit_code_to_report);
    Waiter sequence_done;
    RunSwReportersWithCallback(invocations, base::Version("1.2.3"),
                               sequence_done.SignalClosure());
    sequence_done.Wait();
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
  void ExpectReporterLaunches(int expected_launch_count, bool expect_prompt) {
    EXPECT_EQ(expected_launch_count, reporter_launch_count_);
    EXPECT_EQ(expect_prompt, prompt_trigger_called_);
  }

  // Expects that the reporter has been launched once with each path in
  // |expected_launch_paths|, and TriggerPrompt will be called if and only
  // if |expect_prompt| is true.
  void ExpectReporterLaunches(
      const std::vector<base::FilePath>& expected_launch_paths,
      bool expect_prompt) {
    ExpectReporterLaunches(expected_launch_paths.size(), expect_prompt);
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

  bool SeedIndicatesPromptEnabled() {
    return incoming_seed_.empty() || (incoming_seed_ != old_seed_);
  }

  base::Time now_;

  // Test parameters.
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

 private:
  DISALLOW_COPY_AND_ASSIGN(ReporterRunnerTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, NothingFound) {
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(/*expected_launch_count=*/1, /*expect_prompt=*/false);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, CleanupNeeded) {
  RunReporter(chrome_cleaner::kSwReporterCleanupNeeded);
  ExpectReporterLaunches(/*expected_launch_count=*/1,
                         /*expect_prompt=*/SeedIndicatesPromptEnabled());
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, RanRecently) {
  constexpr int kDaysLeft = 1;
  SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns - kDaysLeft);
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(/*expected_launch_count=*/0, /*expect_prompt=*/false);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, Failure) {
  RunReporter(kReporterNotLaunchedExitCode);
  ExpectReporterLaunches(/*expected_launch_count=*/1, /*expect_prompt=*/false);
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, MultipleLaunches) {
  constexpr int kAFewDays = 3;

  const base::FilePath path1(L"path1");
  const base::FilePath path2(L"path2");
  const base::FilePath path3(L"path3");

  SwReporterQueue invocations;
  invocations.push(SwReporterInvocation::FromFilePath(path1));
  invocations.push(SwReporterInvocation::FromFilePath(path2));

  {
    SCOPED_TRACE("Launch 2 times");
    SetDaysSinceLastTriggered(kDaysBetweenSuccessfulSwReporterRuns);
    RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);
    ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);
  }

  // Schedule a launch with 2 elements, then another with the same 2. It should
  // just run 2 times, not 4.
  {
    SCOPED_TRACE("Launch 2 times with retry");
    FastForwardBy(
        base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns));
    RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);
    ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);

    RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);
    ExpectReporterLaunches({}, /*expect_prompt=*/false);
  }

  {
    SCOPED_TRACE("Attempt to launch after a few days");
    FastForwardBy(base::TimeDelta::FromDays(kAFewDays));
    RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);
    ExpectReporterLaunches({}, /*expect_prompt=*/false);

    FastForwardBy(base::TimeDelta::FromDays(
        kDaysBetweenSuccessfulSwReporterRuns - kAFewDays));
    RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);
    ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);
  }

  // Second launch should not occur after a failure.
  {
    SCOPED_TRACE("Launch multiple times with failure");
    FastForwardBy(
        base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns));
    RunReporterQueue(kReporterNotLaunchedExitCode, invocations);
    ExpectReporterLaunches({path1}, /*expect_prompt=*/false);
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       ReporterLogging_NoSBExtendedReporting) {
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(/*expected_launch_count=*/1, /*expect_prompt=*/false);
  ExpectLoggingSwitches(reporter_launch_parameters_.front(), false);
  ExpectLastTimeSentReportNotSet();
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, ReporterLogging_EnabledFirstRun) {
  EnableSBExtendedReporting();
  // Note: don't set last time sent logs in the local state.
  // SBER is enabled and there is no record in the local state of the last time
  // logs have been sent, so we should send logs in this run.
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(/*expected_launch_count=*/1, /*expect_prompt=*/false);
  ExpectLoggingSwitches(reporter_launch_parameters_.front(), true);
  ExpectLastReportSentInTheLastHour();
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest,
                       ReporterLogging_EnabledNoRecentLogging) {
  // SBER is enabled and last time logs were sent was more than
  // |kDaysBetweenReporterLogsSent| day ago, so we should send logs in this run.
  EnableSBExtendedReporting();
  SetLastTimeSentReport(kDaysBetweenReporterLogsSent + 3);
  RunReporter(chrome_cleaner::kSwReporterNothingFound);
  ExpectReporterLaunches(/*expected_launch_count=*/1, /*expect_prompt=*/false);
  ExpectLoggingSwitches(reporter_launch_parameters_.front(), true);
  ExpectLastReportSentInTheLastHour();
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
  ExpectReporterLaunches(/*expected_launch_count=*/1, /*expect_prompt=*/false);
  ExpectLoggingSwitches(reporter_launch_parameters_.front(), false);
  EXPECT_EQ(last_time_sent_logs, GetLastTimeSentReport());
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
    invocations.push(invocation);
  }
  RunReporterQueue(chrome_cleaner::kSwReporterNothingFound, invocations);

  // SBER is enabled and last time logs were sent was more than
  // |kDaysBetweenReporterLogsSent| day ago, so we should send logs in this run.
  {
    SCOPED_TRACE("first launch");
    first_launch_callback_ =
        base::BindOnce(&ReporterRunnerTest::ExpectLastReportSentInTheLastHour,
                       base::Unretained(this));
    ExpectReporterLaunches({path1, path2}, /*expect_prompt=*/false);
    ExpectLoggingSwitches(reporter_launch_parameters_[0], true);
  }

  // Logs should also be sent for the next run, even though LastTimeSentReport
  // is now recent, because the run is part of the same set of invocations.
  {
    SCOPED_TRACE("second launch");
    ExpectLoggingSwitches(reporter_launch_parameters_[1], true);
    ExpectLastReportSentInTheLastHour();
  }
}

IN_PROC_BROWSER_TEST_P(ReporterRunnerTest, NoSimultaneousInvocations) {
  const base::FilePath path1(L"path1");
  const base::FilePath path2(L"path1");

  ResetReporterRuns(chrome_cleaner::kSwReporterCleanupNeeded);

  // Launch two sequences. The first one will only be released once the second
  // one completes, and then it will unblock the test to continue. After the
  // first sequence ends, checks that only the first reporter invocation was
  // started.
  Waiter first_sequence_done;
  Waiter second_sequence_done;
  RunReporterWithCallback(
      chrome_cleaner::kSwReporterCleanupNeeded, path1,
      base::BindOnce(
          [](Waiter* first_sequence_done, Waiter* second_sequence_done) {
            second_sequence_done->Wait();
            first_sequence_done->Signal();
          },
          &first_sequence_done, &second_sequence_done));
  RunReporterWithCallback(chrome_cleaner::kSwReporterNothingFound, path2,
                          second_sequence_done.SignalClosure());
  first_sequence_done.Wait();

  ExpectReporterLaunches({path1},
                         /*expect_prompt=*/SeedIndicatesPromptEnabled());
}

INSTANTIATE_TEST_CASE_P(
    Default,
    ReporterRunnerTest,
    ::testing::Combine(
        ::testing::Values("", "Seed1"),             // old_seed_
        ::testing::Values("", "Seed1", "Seed2")));  // incoming_seed

}  // namespace safe_browsing
