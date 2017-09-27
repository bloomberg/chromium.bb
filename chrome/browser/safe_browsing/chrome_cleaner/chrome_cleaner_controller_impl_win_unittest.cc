// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_impl_win.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_process_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace safe_browsing {
namespace {

using ::chrome_cleaner::mojom::PromptAcceptance;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using ::testing::_;
using CrashPoint = MockChromeCleanerProcess::CrashPoint;
using IdleReason = ChromeCleanerController::IdleReason;
using State = ChromeCleanerController::State;
using UserResponse = ChromeCleanerController::UserResponse;

// Returns the PromptAcceptance value that ChromeCleanerController is supposed
// to send to the Chrome Cleaner process when ReplyWithUserResponse() is
// called with |user_response|.
PromptAcceptance UserResponseToPromptAcceptance(UserResponse user_response) {
  switch (user_response) {
    case UserResponse::kAcceptedWithLogs:
      return PromptAcceptance::ACCEPTED_WITH_LOGS;
    case UserResponse::kAcceptedWithoutLogs:
      return PromptAcceptance::ACCEPTED_WITHOUT_LOGS;
    case UserResponse::kDenied:  // Fallthrough
    case UserResponse::kDismissed:
      return PromptAcceptance::DENIED;
  }

  NOTREACHED();
  return PromptAcceptance::UNSPECIFIED;
}

class MockChromeCleanerControllerObserver
    : public ChromeCleanerController::Observer {
 public:
  MOCK_METHOD1(OnIdle, void(ChromeCleanerController::IdleReason));
  MOCK_METHOD0(OnScanning, void());
  MOCK_METHOD1(OnInfected, void(const std::set<base::FilePath>&));
  MOCK_METHOD1(OnCleaning, void(const std::set<base::FilePath>&));
  MOCK_METHOD0(OnRebootRequired, void());
  MOCK_METHOD0(OnRebootFailed, void());
  MOCK_METHOD1(OnLogsEnabledChanged, void(bool));
};

enum class MetricsStatus {
  kEnabled,
  kDisabled,
};

// Simple test fixture that passes an invalid process handle back to the
// ChromeCleanerRunner class and is intended for testing simple things like
// command line flags that Chrome sends to the Chrome Cleaner process.
//
// Parameters:
// - metrics_status (MetricsStatus): whether Chrome metrics reporting is
//   enabled.
class ChromeCleanerControllerSimpleTest
    : public testing::TestWithParam<MetricsStatus>,
      public ChromeCleanerRunnerTestDelegate,
      public ChromeCleanerControllerDelegate {
 public:
  ChromeCleanerControllerSimpleTest()
      : command_line_(base::CommandLine::NO_PROGRAM) {}

  ~ChromeCleanerControllerSimpleTest() override {}

  void SetUp() override {
    MetricsStatus metrics_status = GetParam();

    metrics_enabled_ = metrics_status == MetricsStatus::kEnabled;

    SetChromeCleanerRunnerTestDelegateForTesting(this);
    ChromeCleanerControllerImpl::ResetInstanceForTesting();
    ChromeCleanerControllerImpl::GetInstance()->SetDelegateForTesting(this);
    scoped_feature_list_.InitAndEnableFeature(kInBrowserCleanerUIFeature);
  }

  void TearDown() override {
    ChromeCleanerControllerImpl::GetInstance()->SetDelegateForTesting(nullptr);
    SetChromeCleanerRunnerTestDelegateForTesting(nullptr);
  }

  // ChromeCleanerControllerDelegate overrides.

  void FetchAndVerifyChromeCleaner(FetchedCallback fetched_callback) override {
    // In this fixture, we only test the cases when fetching the cleaner
    // executable succeeds.
    std::move(fetched_callback)
        .Run(base::FilePath(FILE_PATH_LITERAL("chrome_cleaner.exe")));
  }

  bool IsMetricsAndCrashReportingEnabled() override { return metrics_enabled_; }

  void TagForResetting(Profile* profile) override {
    // This function should never be called by these tests.
    FAIL();
  }

  void ResetTaggedProfiles(std::vector<Profile*> profiles,
                           base::OnceClosure continuation) override {
    // This function should never be called by these tests.
    FAIL();
  }

  // ChromeCleanerRunnerTestDelegate overrides.

  base::Process LaunchTestProcess(
      const base::CommandLine& command_line,
      const base::LaunchOptions& launch_options) override {
    command_line_ = command_line;
    // Return an invalid process.
    return base::Process();
  }

  void OnCleanerProcessDone(
      const ChromeCleanerRunner::ProcessStatus& process_status) override {}

 protected:
  // We need this because we need UI and IO threads during tests. The thread
  // bundle should be the first member of the class so that it will be destroyed
  // last.
  content::TestBrowserThreadBundle thread_bundle_;
  base::test::ScopedFeatureList scoped_feature_list_;

  bool metrics_enabled_;
  base::CommandLine command_line_;

  StrictMock<MockChromeCleanerControllerObserver> mock_observer_;
};

SwReporterInvocation GetInvocationWithPromptTrigger() {
  SwReporterInvocation invocation = {};
  invocation.supported_behaviours |=
      SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT;
  return invocation;
}

TEST_P(ChromeCleanerControllerSimpleTest, FlagsPassedToCleanerProcess) {
  ChromeCleanerControllerImpl* controller =
      ChromeCleanerControllerImpl::GetInstance();
  ASSERT_TRUE(controller);

  EXPECT_CALL(mock_observer_, OnIdle(_)).Times(1);
  controller->AddObserver(&mock_observer_);
  EXPECT_EQ(controller->state(), State::kIdle);

  EXPECT_CALL(mock_observer_, OnScanning()).Times(1);
  controller->Scan(GetInvocationWithPromptTrigger());
  EXPECT_EQ(controller->state(), State::kScanning);

  base::RunLoop run_loop;
  // The run loop will quit when we get back to the kIdle state, which will
  // happen when launching the Chrome Cleaner process fails (due to the
  // definition of LaunchTestProcess() in the test fixture class).
  EXPECT_CALL(mock_observer_, OnIdle(IdleReason::kScanningFailed))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.QuitWhenIdle(); }));
  run_loop.Run();

  EXPECT_EQ(controller->state(), State::kIdle);
  EXPECT_EQ(metrics_enabled_,
            command_line_.HasSwitch(chrome_cleaner::kUmaUserSwitch));
  EXPECT_EQ(metrics_enabled_, command_line_.HasSwitch(
                                  chrome_cleaner::kEnableCrashReportingSwitch));

  controller->RemoveObserver(&mock_observer_);
}

INSTANTIATE_TEST_CASE_P(All,
                        ChromeCleanerControllerSimpleTest,
                        Values(MetricsStatus::kDisabled,
                               MetricsStatus::kEnabled));

enum class CleanerProcessStatus {
  kFetchFailure,
  kFetchSuccessInvalidProcess,
  kFetchSuccessValidProcess,
};

enum class UwsFoundStatus {
  kNoUwsFound,
  kUwsFoundRebootRequired,
  kUwsFoundNoRebootRequired,
};

// Test fixture that runs a mock Chrome Cleaner process in various
// configurations and mocks the user's response.
class ChromeCleanerControllerTest
    : public testing::TestWithParam<
          std::tuple<CleanerProcessStatus,
                     MockChromeCleanerProcess::CrashPoint,
                     UwsFoundStatus,
                     ChromeCleanerController::UserResponse>>,
      public ChromeCleanerRunnerTestDelegate,
      public ChromeCleanerControllerDelegate {
 public:
  ChromeCleanerControllerTest() = default;
  ~ChromeCleanerControllerTest() override {}

  void SetUp() override {
    std::tie(process_status_, crash_point_, uws_found_status_, user_response_) =
        GetParam();

    cleaner_process_options_.SetDoFindUws(uws_found_status_ !=
                                          UwsFoundStatus::kNoUwsFound);
    cleaner_process_options_.set_reboot_required(
        uws_found_status_ == UwsFoundStatus::kUwsFoundRebootRequired);
    cleaner_process_options_.set_crash_point(crash_point_);
    cleaner_process_options_.set_expected_user_response(
        uws_found_status_ == UwsFoundStatus::kNoUwsFound
            ? PromptAcceptance::DENIED
            : UserResponseToPromptAcceptance(user_response_));

    ChromeCleanerControllerImpl::ResetInstanceForTesting();
    controller_ = ChromeCleanerControllerImpl::GetInstance();
    ASSERT_TRUE(controller_);

    scoped_feature_list_.InitAndEnableFeature(kInBrowserCleanerUIFeature);
    SetChromeCleanerRunnerTestDelegateForTesting(this);
    controller_->SetDelegateForTesting(this);
  }

  void TearDown() override {
    controller_->SetDelegateForTesting(nullptr);
    SetChromeCleanerRunnerTestDelegateForTesting(nullptr);
  }

  // ChromeCleanerControllerDelegate overrides.

  void FetchAndVerifyChromeCleaner(FetchedCallback fetched_callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(fetched_callback),
            process_status_ != CleanerProcessStatus::kFetchFailure
                ? base::FilePath(FILE_PATH_LITERAL("chrome_cleaner.exe"))
                : base::FilePath()));
  }

  bool IsMetricsAndCrashReportingEnabled() override {
    // Returning an arbitrary value since this is not being tested in this
    // fixture.
    return false;
  }

  void TagForResetting(Profile* profile) override {
    profiles_tagged_.push_back(profile);
  }

  void ResetTaggedProfiles(std::vector<Profile*> profiles,
                           base::OnceClosure continuation) override {
    for (Profile* profile : profiles)
      profiles_to_reset_if_tagged_.push_back(profile);
    std::move(continuation).Run();
  }

  // ChromeCleanerRunnerTestDelegate overrides.

  base::Process LaunchTestProcess(
      const base::CommandLine& command_line,
      const base::LaunchOptions& launch_options) override {
    if (process_status_ != CleanerProcessStatus::kFetchSuccessValidProcess)
      return base::Process();

    // Add switches and program name that the test process needs for the multi
    // process tests.
    base::CommandLine test_process_command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    test_process_command_line.AppendArguments(command_line,
                                              /*include_program=*/false);

    cleaner_process_options_.AddSwitchesToCommandLine(
        &test_process_command_line);

    base::Process process = base::SpawnMultiProcessTestChild(
        "MockChromeCleanerProcessMain", test_process_command_line,
        launch_options);

    EXPECT_TRUE(process.IsValid());
    return process;
  }

  void OnCleanerProcessDone(
      const ChromeCleanerRunner::ProcessStatus& process_status) override {
    cleaner_process_status_ = process_status;
  }

  ChromeCleanerController::State ExpectedFinalState() {
    if (process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
        crash_point_ == CrashPoint::kNone &&
        uws_found_status_ == UwsFoundStatus::kUwsFoundRebootRequired &&
        (user_response_ == UserResponse::kAcceptedWithLogs ||
         user_response_ == UserResponse::kAcceptedWithoutLogs)) {
      return State::kRebootRequired;
    }
    return State::kIdle;
  }

  bool ExpectedOnIdleCalled() { return ExpectedFinalState() == State::kIdle; }

  bool ExpectedOnInfectedCalled() {
    return process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
           crash_point_ != CrashPoint::kOnStartup &&
           crash_point_ != CrashPoint::kAfterConnection &&
           uws_found_status_ != UwsFoundStatus::kNoUwsFound;
  }

  bool ExpectedOnCleaningCalled() {
    return ExpectedOnInfectedCalled() &&
           crash_point_ != CrashPoint::kAfterRequestSent &&
           (user_response_ == UserResponse::kAcceptedWithLogs ||
            user_response_ == UserResponse::kAcceptedWithoutLogs);
  }

  bool ExpectedOnRebootRequiredCalled() {
    return ExpectedFinalState() == State::kRebootRequired;
  }

  bool ExpectedUwsFound() { return ExpectedOnInfectedCalled(); }

  bool ExpectedToTagProfile() {
    return process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
           (crash_point_ == CrashPoint::kNone ||
            crash_point_ == CrashPoint::kAfterResponseReceived) &&
           (uws_found_status_ == UwsFoundStatus::kUwsFoundNoRebootRequired ||
            uws_found_status_ == UwsFoundStatus::kUwsFoundRebootRequired) &&
           (user_response_ == UserResponse::kAcceptedWithLogs ||
            user_response_ == UserResponse::kAcceptedWithoutLogs);
  }

  bool ExpectedToResetSettings() {
    return process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
           crash_point_ == CrashPoint::kNone &&
           uws_found_status_ == UwsFoundStatus::kUwsFoundNoRebootRequired &&
           (user_response_ == UserResponse::kAcceptedWithLogs ||
            user_response_ == UserResponse::kAcceptedWithoutLogs);
  }

  ChromeCleanerController::IdleReason ExpectedIdleReason() {
    EXPECT_EQ(ExpectedFinalState(), State::kIdle);

    if (process_status_ != CleanerProcessStatus::kFetchSuccessValidProcess ||
        crash_point_ == CrashPoint::kOnStartup ||
        crash_point_ == CrashPoint::kAfterConnection) {
      return IdleReason::kScanningFailed;
    }

    if (uws_found_status_ == UwsFoundStatus::kNoUwsFound)
      return IdleReason::kScanningFoundNothing;

    if (ExpectedOnInfectedCalled() &&
        (user_response_ == UserResponse::kDenied ||
         user_response_ == UserResponse::kDismissed)) {
      return IdleReason::kUserDeclinedCleanup;
    }

    if (ExpectedOnInfectedCalled() &&
        (user_response_ == UserResponse::kAcceptedWithLogs ||
         user_response_ == UserResponse::kAcceptedWithoutLogs) &&
        crash_point_ == CrashPoint::kAfterResponseReceived) {
      return IdleReason::kCleaningFailed;
    }

    return IdleReason::kCleaningSucceeded;
  }

 protected:
  // We need this because we need UI and IO threads during tests. The thread
  // bundle should be the first member of the class so that it will be destroyed
  // last.
  content::TestBrowserThreadBundle thread_bundle_;
  base::test::ScopedFeatureList scoped_feature_list_;

  CleanerProcessStatus process_status_;
  MockChromeCleanerProcess::CrashPoint crash_point_;
  UwsFoundStatus uws_found_status_;
  ChromeCleanerController::UserResponse user_response_;

  MockChromeCleanerProcess::Options cleaner_process_options_;

  StrictMock<MockChromeCleanerControllerObserver> mock_observer_;
  ChromeCleanerControllerImpl* controller_;
  ChromeCleanerRunner::ProcessStatus cleaner_process_status_;

  std::vector<Profile*> profiles_tagged_;
  std::vector<Profile*> profiles_to_reset_if_tagged_;
};

MULTIPROCESS_TEST_MAIN(MockChromeCleanerProcessMain) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  MockChromeCleanerProcess::Options options;
  EXPECT_TRUE(MockChromeCleanerProcess::Options::FromCommandLine(*command_line,
                                                                 &options));

  std::string chrome_mojo_pipe_token = command_line->GetSwitchValueASCII(
      chrome_cleaner::kChromeMojoPipeTokenSwitch);
  EXPECT_FALSE(chrome_mojo_pipe_token.empty());

  // Since failures in any of the above calls to EXPECT_*() do not actually fail
  // the test, we need to ensure that we return an exit code to indicate test
  // failure in such cases.
  if (::testing::Test::HasFailure())
    return MockChromeCleanerProcess::kInternalTestFailureExitCode;

  MockChromeCleanerProcess mock_cleaner_process(options,
                                                chrome_mojo_pipe_token);
  return mock_cleaner_process.Run();
}

TEST_P(ChromeCleanerControllerTest, WithMockCleanerProcess) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  constexpr char kTestProfileName1[] = "Test 1";
  constexpr char kTestProfileName2[] = "Test 2";

  Profile* profile1 = profile_manager.CreateTestingProfile(kTestProfileName1);
  ASSERT_TRUE(profile1);
  Profile* profile2 = profile_manager.CreateTestingProfile(kTestProfileName2);
  ASSERT_TRUE(profile2);

  const int num_profiles =
      profile_manager.profile_manager()->GetNumberOfProfiles();
  ASSERT_EQ(2, num_profiles);

  EXPECT_CALL(mock_observer_, OnIdle(_)).Times(1);
  controller_->AddObserver(&mock_observer_);
  EXPECT_EQ(controller_->state(), State::kIdle);

  EXPECT_CALL(mock_observer_, OnScanning()).Times(1);
  controller_->Scan(GetInvocationWithPromptTrigger());
  EXPECT_EQ(controller_->state(), State::kScanning);

  base::RunLoop run_loop;

  std::set<base::FilePath> files_to_delete_on_infected;
  std::set<base::FilePath> files_to_delete_on_cleaning;

  if (ExpectedOnIdleCalled()) {
    EXPECT_CALL(mock_observer_, OnIdle(ExpectedIdleReason()))
        .WillOnce(
            InvokeWithoutArgs([&run_loop]() { run_loop.QuitWhenIdle(); }));
  }

  if (ExpectedOnInfectedCalled()) {
    EXPECT_CALL(mock_observer_, OnInfected(_))
        .WillOnce(DoAll(SaveArg<0>(&files_to_delete_on_infected),
                        InvokeWithoutArgs([this, profile1]() {
                          controller_->ReplyWithUserResponse(profile1,
                                                             user_response_);
                        })));
    // Since logs upload is enabled by default, OnLogsEnabledChanged() will be
    // called only if the user response is kAcceptedWithoutLogs.
    if (user_response_ == UserResponse::kAcceptedWithoutLogs)
      EXPECT_CALL(mock_observer_, OnLogsEnabledChanged(false));
  }

  if (ExpectedOnCleaningCalled()) {
    EXPECT_CALL(mock_observer_, OnCleaning(_))
        .WillOnce(SaveArg<0>(&files_to_delete_on_cleaning));
  }

  if (ExpectedOnRebootRequiredCalled()) {
    EXPECT_CALL(mock_observer_, OnRebootRequired())
        .WillOnce(
            InvokeWithoutArgs([&run_loop]() { run_loop.QuitWhenIdle(); }));
  }

  // Assert here that we expect at least one of OnIdle or OnRebootRequired to be
  // called, since otherwise, the test is set up incorrectly and is expected to
  // never stop.
  ASSERT_TRUE(ExpectedOnIdleCalled() || ExpectedOnRebootRequiredCalled());
  run_loop.Run();
  // Also ensure that we wait until the mock cleaner process has finished and
  // that all tasks that posted by ChromeCleanerRunner have run.
  content::RunAllTasksUntilIdle();

  EXPECT_NE(cleaner_process_status_.exit_code,
            MockChromeCleanerProcess::kInternalTestFailureExitCode);
  EXPECT_EQ(controller_->state(), ExpectedFinalState());
  EXPECT_EQ(!files_to_delete_on_infected.empty(), ExpectedUwsFound());
  EXPECT_EQ(!files_to_delete_on_cleaning.empty(),
            ExpectedUwsFound() && ExpectedOnCleaningCalled());
  if (!files_to_delete_on_infected.empty() &&
      !files_to_delete_on_cleaning.empty()) {
    EXPECT_EQ(files_to_delete_on_infected, files_to_delete_on_cleaning);
  }

  std::vector<Profile*> expected_tagged;
  if (ExpectedToTagProfile())
    expected_tagged.push_back(profile1);
  EXPECT_THAT(expected_tagged, UnorderedElementsAreArray(profiles_tagged_));

  std::vector<Profile*> expected_reset_if_tagged;
  if (ExpectedToResetSettings()) {
    expected_reset_if_tagged.push_back(profile1);
    expected_reset_if_tagged.push_back(profile2);
  }
  EXPECT_THAT(expected_reset_if_tagged,
              UnorderedElementsAreArray(profiles_to_reset_if_tagged_));

  controller_->RemoveObserver(&mock_observer_);
}

INSTANTIATE_TEST_CASE_P(
    All,
    ChromeCleanerControllerTest,
    Combine(Values(CleanerProcessStatus::kFetchFailure,
                   CleanerProcessStatus::kFetchSuccessInvalidProcess,
                   CleanerProcessStatus::kFetchSuccessValidProcess),
            Values(CrashPoint::kNone,
                   CrashPoint::kOnStartup,
                   CrashPoint::kAfterConnection,
                   // CrashPoint::kAfterRequestSent is not used because we
                   // cannot ensure the order between the Mojo request being
                   // received by Chrome and the connection being lost.
                   CrashPoint::kAfterResponseReceived),
            Values(UwsFoundStatus::kNoUwsFound,
                   UwsFoundStatus::kUwsFoundRebootRequired,
                   UwsFoundStatus::kUwsFoundNoRebootRequired),
            Values(UserResponse::kAcceptedWithLogs,
                   UserResponse::kAcceptedWithoutLogs,
                   UserResponse::kDenied,
                   UserResponse::kDismissed)));

}  // namespace
}  // namespace safe_browsing
