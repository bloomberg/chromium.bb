// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace password_manager {

PasswordFormMetricsRecorder::PasswordFormMetricsRecorder(
    bool is_main_frame_secure)
    : is_main_frame_secure_(is_main_frame_secure) {}

PasswordFormMetricsRecorder::~PasswordFormMetricsRecorder() {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ActionsTakenV3", GetActionsTaken(),
                            kMaxNumActionsTaken);

  // Use the visible main frame URL at the time the PasswordFormManager
  // is created, in case a navigation has already started and the
  // visible URL has changed.
  if (!is_main_frame_secure_) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.ActionsTakenOnNonSecureForm",
                              GetActionsTaken(), kMaxNumActionsTaken);
  }

  if (submit_result_ == kSubmitResultNotSubmitted) {
    if (has_generated_password_) {
      metrics_util::LogPasswordGenerationSubmissionEvent(
          metrics_util::PASSWORD_NOT_SUBMITTED);
    } else if (generation_available_) {
      metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
          metrics_util::PASSWORD_NOT_SUBMITTED);
    }
  }
}

void PasswordFormMetricsRecorder::MarkGenerationAvailable() {
  generation_available_ = true;
}

void PasswordFormMetricsRecorder::SetHasGeneratedPassword(
    bool has_generated_password) {
  has_generated_password_ = has_generated_password;
}

void PasswordFormMetricsRecorder::SetManagerAction(
    ManagerAction manager_action) {
  manager_action_ = manager_action;
}

void PasswordFormMetricsRecorder::SetUserAction(UserAction user_action) {
  if (user_action == UserAction::kChoose) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_UsedNonDefaultUsername"));
  } else if (user_action == UserAction::kChoosePslMatch) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_ChoseSubdomainPassword"));
  } else if (user_action == UserAction::kOverridePassword) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_LoggedInWithNewPassword"));
  } else if (user_action == UserAction::kOverrideUsernameAndPassword) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_LoggedInWithNewUsername"));
  } else {
    NOTREACHED();
  }
  user_action_ = user_action;
}

void PasswordFormMetricsRecorder::LogSubmitPassed() {
  if (submit_result_ != kSubmitResultFailed) {
    if (has_generated_password_) {
      metrics_util::LogPasswordGenerationSubmissionEvent(
          metrics_util::PASSWORD_SUBMITTED);
    } else if (generation_available_) {
      metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
          metrics_util::PASSWORD_SUBMITTED);
    }
  }
  base::RecordAction(base::UserMetricsAction("PasswordManager_LoginPassed"));
  submit_result_ = kSubmitResultPassed;
}

void PasswordFormMetricsRecorder::LogSubmitFailed() {
  if (has_generated_password_) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::GENERATED_PASSWORD_FORCE_SAVED);
  } else if (generation_available_) {
    metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
        metrics_util::PASSWORD_SUBMISSION_FAILED);
  }
  base::RecordAction(base::UserMetricsAction("PasswordManager_LoginFailed"));
  submit_result_ = kSubmitResultFailed;
}

int PasswordFormMetricsRecorder::GetActionsTakenNew() const {
  // Merge kManagerActionNone and kManagerActionBlacklisted_Obsolete. This
  // lowers the number of histogram buckets used by 33%.
  ManagerActionNew manager_action_new =
      (manager_action_ == kManagerActionAutofilled)
          ? kManagerActionNewAutofilled
          : kManagerActionNewNone;

  return static_cast<int>(user_action_) +
         static_cast<int>(UserAction::kMax) *
             (manager_action_new + kManagerActionNewMax * submit_result_);
}

int PasswordFormMetricsRecorder::GetActionsTaken() const {
  return static_cast<int>(user_action_) +
         static_cast<int>(UserAction::kMax) *
             (manager_action_ + kManagerActionMax * submit_result_);
}

}  // namespace password_manager
