// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

#include "base/logging.h"
#include "base/test/histogram_tester.h"
#include "base/test/user_action_tester.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

// Test the metrics recorded around password generation and the user's
// interaction with the offer to generate passwords.
TEST(PasswordFormMetricsRecorder, Generation) {
  static constexpr struct {
    bool generation_available;
    bool has_generated_password;
    PasswordFormMetricsRecorder::SubmitResult submission;
  } kTests[] = {
      {false, false, PasswordFormMetricsRecorder::kSubmitResultNotSubmitted},
      {true, false, PasswordFormMetricsRecorder::kSubmitResultNotSubmitted},
      {true, true, PasswordFormMetricsRecorder::kSubmitResultNotSubmitted},
      {false, false, PasswordFormMetricsRecorder::kSubmitResultFailed},
      {true, false, PasswordFormMetricsRecorder::kSubmitResultFailed},
      {true, true, PasswordFormMetricsRecorder::kSubmitResultFailed},
      {false, false, PasswordFormMetricsRecorder::kSubmitResultPassed},
      {true, false, PasswordFormMetricsRecorder::kSubmitResultPassed},
      {true, true, PasswordFormMetricsRecorder::kSubmitResultPassed},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "generation_available=" << test.generation_available
                 << ", has_generated_password=" << test.has_generated_password
                 << ", submission" << test.submission);

    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
    // on destruction.
    {
      PasswordFormMetricsRecorder recorder(/*is_main_frame_secure*/ true);
      if (test.generation_available)
        recorder.MarkGenerationAvailable();
      recorder.SetHasGeneratedPassword(test.has_generated_password);
      switch (test.submission) {
        case PasswordFormMetricsRecorder::kSubmitResultNotSubmitted:
          // Do nothing.
          break;
        case PasswordFormMetricsRecorder::kSubmitResultFailed:
          recorder.LogSubmitFailed();
          break;
        case PasswordFormMetricsRecorder::kSubmitResultPassed:
          recorder.LogSubmitPassed();
          break;
        case PasswordFormMetricsRecorder::kSubmitResultMax:
          NOTREACHED();
      }
    }

    EXPECT_EQ(
        test.submission == PasswordFormMetricsRecorder::kSubmitResultFailed ? 1
                                                                            : 0,
        user_action_tester.GetActionCount("PasswordManager_LoginFailed"));

    EXPECT_EQ(
        test.submission == PasswordFormMetricsRecorder::kSubmitResultPassed ? 1
                                                                            : 0,
        user_action_tester.GetActionCount("PasswordManager_LoginPassed"));

    if (test.has_generated_password) {
      switch (test.submission) {
        case PasswordFormMetricsRecorder::kSubmitResultNotSubmitted:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::PASSWORD_NOT_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultFailed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::GENERATED_PASSWORD_FORCE_SAVED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultPassed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::PASSWORD_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultMax:
          NOTREACHED();
      }
    }

    if (!test.has_generated_password && test.generation_available) {
      switch (test.submission) {
        case PasswordFormMetricsRecorder::kSubmitResultNotSubmitted:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_NOT_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultFailed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_SUBMISSION_FAILED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultPassed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultMax:
          NOTREACHED();
      }
    }
  }
}

// Test the recording of metrics around manager_action, user_action, and
// submit_result.
TEST(PasswordFormMetricsRecorder, Actions) {
  static constexpr struct {
    // Stimuli:
    bool is_main_frame_secure;
    PasswordFormMetricsRecorder::ManagerAction manager_action;
    UserAction user_action;
    PasswordFormMetricsRecorder::SubmitResult submit_result;
    // Expectations:
    // Histogram bucket for PasswordManager.ActionsTakenV3 and
    // PasswordManager.ActionsTakenOnNonSecureForm.
    int32_t actions_taken;
    // Result of GetActionsTakenNew.
    int32_t actions_taken_new;
  } kTests[] = {
      // Test values of manager_action.
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kNone /*0*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 0, 0},
      {true, PasswordFormMetricsRecorder::kManagerActionAutofilled /*1*/,
       UserAction::kNone /*0*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 5, 5},
      // Test values of user_action.
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kChoose /*1*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 1, 1},
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kChoosePslMatch /*2*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 2, 2},
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kOverridePassword /*3*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 3, 3},
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kOverrideUsernameAndPassword /*4*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 4, 4},
      // Test values of submit_result.
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kNone /*0*/,
       PasswordFormMetricsRecorder::kSubmitResultFailed /*1*/, 15, 10},
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kNone /*0*/,
       PasswordFormMetricsRecorder::kSubmitResultPassed /*2*/, 30, 20},
      // Test combination.
      {true, PasswordFormMetricsRecorder::kManagerActionAutofilled /*1*/,
       UserAction::kOverrideUsernameAndPassword /*4*/,
       PasswordFormMetricsRecorder::kSubmitResultFailed /*2*/, 24, 19},
      // Test non-secure form.
      {false, PasswordFormMetricsRecorder::kManagerActionAutofilled /*1*/,
       UserAction::kOverrideUsernameAndPassword /*4*/,
       PasswordFormMetricsRecorder::kSubmitResultFailed /*2*/, 24, 19},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "is_main_frame_secure=" << test.is_main_frame_secure
                 << ", manager_action=" << test.manager_action
                 << ", user_action" << static_cast<int>(test.user_action)
                 << ", submit_result=" << test.submit_result);

    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
    // on destruction.
    {
      PasswordFormMetricsRecorder recorder(test.is_main_frame_secure);

      recorder.SetManagerAction(test.manager_action);
      if (test.user_action != UserAction::kNone)
        recorder.SetUserAction(test.user_action);
      if (test.submit_result ==
          PasswordFormMetricsRecorder::kSubmitResultFailed) {
        recorder.LogSubmitFailed();
      } else if (test.submit_result ==
                 PasswordFormMetricsRecorder::kSubmitResultPassed) {
        recorder.LogSubmitPassed();
      }

      EXPECT_EQ(test.actions_taken_new, recorder.GetActionsTakenNew());
    }

    EXPECT_THAT(
        histogram_tester.GetAllSamples("PasswordManager.ActionsTakenV3"),
        ::testing::ElementsAre(base::Bucket(test.actions_taken, 1)));
    if (!test.is_main_frame_secure) {
      EXPECT_THAT(histogram_tester.GetAllSamples(
                      "PasswordManager.ActionsTakenOnNonSecureForm"),
                  ::testing::ElementsAre(base::Bucket(test.actions_taken, 1)));
    }

    switch (test.user_action) {
      case UserAction::kNone:
        break;
      case UserAction::kChoose:
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "PasswordManager_UsedNonDefaultUsername"));
        break;
      case UserAction::kChoosePslMatch:
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "PasswordManager_ChoseSubdomainPassword"));
        break;
      case UserAction::kOverridePassword:
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "PasswordManager_LoggedInWithNewPassword"));
        break;
      case UserAction::kOverrideUsernameAndPassword:
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "PasswordManager_LoggedInWithNewUsername"));
        break;
      case UserAction::kMax:
        break;
    }
  }
}

// Test that in the case of a sequence of user actions, only the last one is
// recorded in ActionsV3 but all are recorded as UMA user actions.
TEST(PasswordFormMetricsRecorder, ActionSequence) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
  // on destruction.
  {
    PasswordFormMetricsRecorder recorder(/*is_main_frame_secure*/ true);
    recorder.SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionAutofilled);
    recorder.SetUserAction(UserAction::kChoosePslMatch);
    recorder.SetUserAction(UserAction::kOverrideUsernameAndPassword);
    recorder.LogSubmitPassed();
  }

  EXPECT_THAT(histogram_tester.GetAllSamples("PasswordManager.ActionsTakenV3"),
              ::testing::ElementsAre(base::Bucket(39, 1)));

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "PasswordManager_ChoseSubdomainPassword"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "PasswordManager_LoggedInWithNewUsername"));
}

TEST(PasswordFormMetricsRecorder, SubmittedFormType) {
  static constexpr struct {
    // Stimuli:
    bool is_main_frame_secure;
    PasswordFormMetricsRecorder::SubmittedFormType form_type;
    // Expectations:
    // Expectation for PasswordManager.SubmittedFormType:
    int expected_submitted_form_type;
    // Expectation for PasswordManager.SubmittedNonSecureFormType:
    int expected_submitted_non_secure_form_type;
  } kTests[] = {
      {false, PasswordFormMetricsRecorder::kSubmittedFormTypeUnspecified, 0, 0},
      {true, PasswordFormMetricsRecorder::kSubmittedFormTypeUnspecified, 0, 0},
      {false, PasswordFormMetricsRecorder::kSubmittedFormTypeLogin, 1, 1},
      {true, PasswordFormMetricsRecorder::kSubmittedFormTypeLogin, 1, 0},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "is_main_frame_secure=" << test.is_main_frame_secure
                 << ", form_type=" << test.form_type);

    base::HistogramTester histogram_tester;

    // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
    // on destruction.
    {
      PasswordFormMetricsRecorder recorder(test.is_main_frame_secure);
      recorder.SetSubmittedFormType(test.form_type);
    }

    if (test.expected_submitted_form_type) {
      histogram_tester.ExpectBucketCount("PasswordManager.SubmittedFormType",
                                         test.form_type,
                                         test.expected_submitted_form_type);
    } else {
      histogram_tester.ExpectTotalCount("PasswordManager.SubmittedFormType", 0);
    }

    if (test.expected_submitted_non_secure_form_type) {
      histogram_tester.ExpectBucketCount(
          "PasswordManager.SubmittedNonSecureFormType", test.form_type,
          test.expected_submitted_non_secure_form_type);
    } else {
      histogram_tester.ExpectTotalCount(
          "PasswordManager.SubmittedNonSecureFormType", 0);
    }
  }
}

}  // namespace password_manager
