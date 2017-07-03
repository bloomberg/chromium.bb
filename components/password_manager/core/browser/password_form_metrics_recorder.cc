// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

using autofill::PasswordForm;

namespace password_manager {

PasswordFormMetricsRecorder::PasswordFormMetricsRecorder(
    bool is_main_frame_secure,
    std::unique_ptr<ukm::UkmEntryBuilder> ukm_entry_builder)
    : is_main_frame_secure_(is_main_frame_secure),
      ukm_entry_builder_(std::move(ukm_entry_builder)) {}

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
    RecordUkmMetric(internal::kUkmSubmissionObserved, 0 /*false*/);
  }

  if (submitted_form_type_ != kSubmittedFormTypeUnspecified) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.SubmittedFormType",
                              submitted_form_type_, kSubmittedFormTypeMax);
    if (!is_main_frame_secure_) {
      UMA_HISTOGRAM_ENUMERATION("PasswordManager.SubmittedNonSecureFormType",
                                submitted_form_type_, kSubmittedFormTypeMax);
    }

    RecordUkmMetric(internal::kUkmSubmissionFormType, submitted_form_type_);
  }
}

// static
std::unique_ptr<ukm::UkmEntryBuilder>
PasswordFormMetricsRecorder::CreateUkmEntryBuilder(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id) {
  if (!ukm_recorder)
    return nullptr;
  return ukm_recorder->GetEntryBuilder(source_id, "PasswordForm");
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
  RecordUkmMetric(internal::kUkmSubmissionObserved, 1 /*true*/);
  RecordUkmMetric(internal::kUkmSubmissionResult, kSubmitResultPassed);
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
  RecordUkmMetric(internal::kUkmSubmissionObserved, 1 /*true*/);
  RecordUkmMetric(internal::kUkmSubmissionResult, kSubmitResultFailed);
  submit_result_ = kSubmitResultFailed;
}

void PasswordFormMetricsRecorder::SetSubmittedFormType(
    SubmittedFormType form_type) {
  submitted_form_type_ = form_type;
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

void PasswordFormMetricsRecorder::RecordHistogramsOnSuppressedAccounts(
    bool observed_form_origin_has_cryptographic_scheme,
    const FormFetcher& form_fetcher,
    const PasswordForm& pending_credentials) {
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.QueryingSuppressedAccountsFinished",
                        form_fetcher.DidCompleteQueryingSuppressedForms());

  if (!form_fetcher.DidCompleteQueryingSuppressedForms())
    return;

  SuppressedAccountExistence best_match = kSuppressedAccountNone;

  if (!observed_form_origin_has_cryptographic_scheme) {
    best_match = GetBestMatchingSuppressedAccount(
        form_fetcher.GetSuppressedHTTPSForms(), PasswordForm::TYPE_GENERATED,
        pending_credentials);
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.SuppressedAccount.Generated.HTTPSNotHTTP",
        GetHistogramSampleForSuppressedAccounts(best_match),
        kMaxSuppressedAccountStats);
    RecordUkmMetric("SuppressedAccount.Generated.HTTPSNotHTTP", best_match);

    best_match = GetBestMatchingSuppressedAccount(
        form_fetcher.GetSuppressedHTTPSForms(), PasswordForm::TYPE_MANUAL,
        pending_credentials);
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.SuppressedAccount.Manual.HTTPSNotHTTP",
        GetHistogramSampleForSuppressedAccounts(best_match),
        kMaxSuppressedAccountStats);
    RecordUkmMetric("SuppressedAccount.Manual.HTTPSNotHTTP", best_match);
  }

  best_match = GetBestMatchingSuppressedAccount(
      form_fetcher.GetSuppressedPSLMatchingForms(),
      PasswordForm::TYPE_GENERATED, pending_credentials);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuppressedAccount.Generated.PSLMatching",
      GetHistogramSampleForSuppressedAccounts(best_match),
      kMaxSuppressedAccountStats);
  RecordUkmMetric("SuppressedAccount.Generated.PSLMatching", best_match);
  best_match = GetBestMatchingSuppressedAccount(
      form_fetcher.GetSuppressedPSLMatchingForms(), PasswordForm::TYPE_MANUAL,
      pending_credentials);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuppressedAccount.Manual.PSLMatching",
      GetHistogramSampleForSuppressedAccounts(best_match),
      kMaxSuppressedAccountStats);
  RecordUkmMetric("SuppressedAccount.Manual.PSLMatching", best_match);

  best_match = GetBestMatchingSuppressedAccount(
      form_fetcher.GetSuppressedSameOrganizationNameForms(),
      PasswordForm::TYPE_GENERATED, pending_credentials);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuppressedAccount.Generated.SameOrganizationName",
      GetHistogramSampleForSuppressedAccounts(best_match),
      kMaxSuppressedAccountStats);
  RecordUkmMetric("SuppressedAccount.Generated.SameOrganizationName",
                  best_match);
  best_match = GetBestMatchingSuppressedAccount(
      form_fetcher.GetSuppressedSameOrganizationNameForms(),
      PasswordForm::TYPE_MANUAL, pending_credentials);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuppressedAccount.Manual.SameOrganizationName",
      GetHistogramSampleForSuppressedAccounts(best_match),
      kMaxSuppressedAccountStats);
  RecordUkmMetric("SuppressedAccount.Manual.SameOrganizationName", best_match);
}

PasswordFormMetricsRecorder::SuppressedAccountExistence
PasswordFormMetricsRecorder::GetBestMatchingSuppressedAccount(
    const std::vector<const PasswordForm*>& suppressed_forms,
    PasswordForm::Type manual_or_generated,
    const PasswordForm& pending_credentials) const {
  SuppressedAccountExistence best_matching_account = kSuppressedAccountNone;
  for (const PasswordForm* form : suppressed_forms) {
    if (form->type != manual_or_generated)
      continue;

    SuppressedAccountExistence current_account;
    if (pending_credentials.password_value.empty())
      current_account = kSuppressedAccountExists;
    else if (form->username_value != pending_credentials.username_value)
      current_account = kSuppressedAccountExistsDifferentUsername;
    else if (form->password_value != pending_credentials.password_value)
      current_account = kSuppressedAccountExistsSameUsername;
    else
      current_account = kSuppressedAccountExistsSameUsernameAndPassword;

    best_matching_account = std::max(best_matching_account, current_account);
  }
  return best_matching_account;
}

int PasswordFormMetricsRecorder::GetHistogramSampleForSuppressedAccounts(
    SuppressedAccountExistence best_matching_account) const {
  // Encoding: most significant digit is the |best_matching_account|.
  int mixed_base_encoding = 0;
  mixed_base_encoding += best_matching_account;
  mixed_base_encoding *= PasswordFormMetricsRecorder::kMaxNumActionsTakenNew;
  mixed_base_encoding += GetActionsTakenNew();
  DCHECK_LT(mixed_base_encoding, kMaxSuppressedAccountStats);
  return mixed_base_encoding;
}

void PasswordFormMetricsRecorder::RecordUkmMetric(const char* metric_name,
                                                  int64_t value) {
  if (ukm_entry_builder_)
    ukm_entry_builder_->AddMetric(metric_name, value);
}

}  // namespace password_manager
