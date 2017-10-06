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

namespace {

PasswordFormMetricsRecorder::BubbleDismissalReason GetBubbleDismissalReason(
    metrics_util::UIDismissalReason ui_dismissal_reason) {
  using BubbleDismissalReason =
      PasswordFormMetricsRecorder::BubbleDismissalReason;
  switch (ui_dismissal_reason) {
    // Accepted by user.
    case metrics_util::CLICKED_SAVE:
      return BubbleDismissalReason::kAccepted;

    // Declined by user.
    case metrics_util::CLICKED_CANCEL:
    case metrics_util::CLICKED_NEVER:
      return BubbleDismissalReason::kDeclined;

    // Ignored by user.
    case metrics_util::NO_DIRECT_INTERACTION:
      return BubbleDismissalReason::kIgnored;

    // Ignore these for metrics collection:
    case metrics_util::CLICKED_MANAGE:
    case metrics_util::CLICKED_DONE:
    case metrics_util::CLICKED_OK:
    case metrics_util::CLICKED_BRAND_NAME:
    case metrics_util::CLICKED_PASSWORDS_DASHBOARD:
    case metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT:
      break;

    // These should not reach here:
    case metrics_util::CLICKED_UNBLACKLIST_OBSOLETE:
    case metrics_util::CLICKED_CREDENTIAL_OBSOLETE:
    case metrics_util::AUTO_SIGNIN_TOAST_CLICKED_OBSOLETE:
    case metrics_util::NUM_UI_RESPONSES:
      NOTREACHED();
      break;
  }
  return BubbleDismissalReason::kUnknown;
}

}  // namespace

PasswordFormMetricsRecorder::PasswordFormMetricsRecorder(
    bool is_main_frame_secure,
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    const GURL& main_frame_url)
    : is_main_frame_secure_(is_main_frame_secure),
      ukm_recorder_(ukm_recorder),
      source_id_(source_id),
      main_frame_url_(main_frame_url),
      ukm_entry_builder_(source_id) {}

PasswordFormMetricsRecorder::~PasswordFormMetricsRecorder() {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ActionsTakenV3", GetActionsTaken(),
                            kMaxNumActionsTaken);
  ukm_entry_builder_.SetUser_ActionSimplified(
      static_cast<int64_t>(user_action_));

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
    ukm_entry_builder_.SetSubmission_Observed(0 /*false*/);
  }

  if (submitted_form_type_ != kSubmittedFormTypeUnspecified) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.SubmittedFormType",
                              submitted_form_type_, kSubmittedFormTypeMax);
    if (!is_main_frame_secure_) {
      UMA_HISTOGRAM_ENUMERATION("PasswordManager.SubmittedNonSecureFormType",
                                submitted_form_type_, kSubmittedFormTypeMax);
    }

    ukm_entry_builder_.SetSubmission_SubmittedFormType(submitted_form_type_);
  }

  ukm_entry_builder_.SetUpdating_Prompt_Shown(update_prompt_shown_);
  ukm_entry_builder_.SetSaving_Prompt_Shown(save_prompt_shown_);

  // TODO(crbug/755407): This shouldn't depend on UKM keeping repeated metrics.
  for (const DetailedUserAction& action : one_time_report_user_actions_)
    ukm_entry_builder_.SetUser_Action(static_cast<int64_t>(action));

  ukm_entry_builder_.Record(ukm_recorder_);

  // Bind |main_frame_url_| to |source_id_| directly before sending the content
  // of |ukm_recorder_| to ensure that the binding has not been purged already.
  if (ukm_recorder_)
    ukm_recorder_->UpdateSourceURL(source_id_, main_frame_url_);
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
  ukm_entry_builder_.SetSubmission_Observed(1 /*true*/);
  ukm_entry_builder_.SetSubmission_SubmissionResult(kSubmitResultPassed);
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
  ukm_entry_builder_.SetSubmission_Observed(1 /*true*/);
  ukm_entry_builder_.SetSubmission_SubmissionResult(kSubmitResultFailed);
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

void PasswordFormMetricsRecorder::RecordDetailedUserAction(
    PasswordFormMetricsRecorder::DetailedUserAction action) {
  // One-time actions are collected and reported during destruction of the
  // PasswordFormMetricsRecorder.
  if (!IsRepeatedUserAction(action)) {
    one_time_report_user_actions_.insert(action);
    return;
  }
  // Repeated actions can be reported immediately.
  // TODO(crbug/755407): This shouldn't depend on UKM keeping repeated metrics.
  ukm_entry_builder_.SetUser_Action(static_cast<int64_t>(action));
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
    ukm_entry_builder_.SetSuppressedAccount_Generated_HTTPSNotHTTP(best_match);

    best_match = GetBestMatchingSuppressedAccount(
        form_fetcher.GetSuppressedHTTPSForms(), PasswordForm::TYPE_MANUAL,
        pending_credentials);
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.SuppressedAccount.Manual.HTTPSNotHTTP",
        GetHistogramSampleForSuppressedAccounts(best_match),
        kMaxSuppressedAccountStats);
    ukm_entry_builder_.SetSuppressedAccount_Manual_HTTPSNotHTTP(best_match);
  }

  best_match = GetBestMatchingSuppressedAccount(
      form_fetcher.GetSuppressedPSLMatchingForms(),
      PasswordForm::TYPE_GENERATED, pending_credentials);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuppressedAccount.Generated.PSLMatching",
      GetHistogramSampleForSuppressedAccounts(best_match),
      kMaxSuppressedAccountStats);
  ukm_entry_builder_.SetSuppressedAccount_Generated_PSLMatching(best_match);
  best_match = GetBestMatchingSuppressedAccount(
      form_fetcher.GetSuppressedPSLMatchingForms(), PasswordForm::TYPE_MANUAL,
      pending_credentials);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuppressedAccount.Manual.PSLMatching",
      GetHistogramSampleForSuppressedAccounts(best_match),
      kMaxSuppressedAccountStats);
  ukm_entry_builder_.SetSuppressedAccount_Manual_PSLMatching(best_match);

  best_match = GetBestMatchingSuppressedAccount(
      form_fetcher.GetSuppressedSameOrganizationNameForms(),
      PasswordForm::TYPE_GENERATED, pending_credentials);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuppressedAccount.Generated.SameOrganizationName",
      GetHistogramSampleForSuppressedAccounts(best_match),
      kMaxSuppressedAccountStats);
  ukm_entry_builder_.SetSuppressedAccount_Generated_SameOrganizationName(
      best_match);
  best_match = GetBestMatchingSuppressedAccount(
      form_fetcher.GetSuppressedSameOrganizationNameForms(),
      PasswordForm::TYPE_MANUAL, pending_credentials);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuppressedAccount.Manual.SameOrganizationName",
      GetHistogramSampleForSuppressedAccounts(best_match),
      kMaxSuppressedAccountStats);
  ukm_entry_builder_.SetSuppressedAccount_Manual_SameOrganizationName(
      best_match);

  if (current_bubble_ != CurrentBubbleOfInterest::kNone)
    RecordUIDismissalReason(metrics_util::NO_DIRECT_INTERACTION);
}

void PasswordFormMetricsRecorder::RecordPasswordBubbleShown(
    metrics_util::CredentialSourceType credential_source_type,
    metrics_util::UIDisplayDisposition display_disposition) {
  if (credential_source_type == metrics_util::CredentialSourceType::kUnknown)
    return;
  DCHECK_EQ(CurrentBubbleOfInterest::kNone, current_bubble_);
  BubbleTrigger automatic_trigger_type =
      credential_source_type ==
              metrics_util::CredentialSourceType::kPasswordManager
          ? BubbleTrigger::kPasswordManagerSuggestionAutomatic
          : BubbleTrigger::kCredentialManagementAPIAutomatic;
  BubbleTrigger manual_trigger_type =
      credential_source_type ==
              metrics_util::CredentialSourceType::kPasswordManager
          ? BubbleTrigger::kPasswordManagerSuggestionManual
          : BubbleTrigger::kCredentialManagementAPIManual;

  switch (display_disposition) {
    // New credential cases:
    case metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING:
      current_bubble_ = CurrentBubbleOfInterest::kSaveBubble;
      save_prompt_shown_ = true;
      ukm_entry_builder_.SetSaving_Prompt_Trigger(
          static_cast<int64_t>(automatic_trigger_type));
      break;
    case metrics_util::MANUAL_WITH_PASSWORD_PENDING:
      current_bubble_ = CurrentBubbleOfInterest::kSaveBubble;
      save_prompt_shown_ = true;
      ukm_entry_builder_.SetSaving_Prompt_Trigger(
          static_cast<int64_t>(manual_trigger_type));
      break;

    // Update cases:
    case metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE:
      current_bubble_ = CurrentBubbleOfInterest::kUpdateBubble;
      update_prompt_shown_ = true;
      ukm_entry_builder_.SetUpdating_Prompt_Trigger(
          static_cast<int64_t>(automatic_trigger_type));
      break;
    case metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE:
      current_bubble_ = CurrentBubbleOfInterest::kUpdateBubble;
      update_prompt_shown_ = true;
      ukm_entry_builder_.SetUpdating_Prompt_Trigger(
          static_cast<int64_t>(manual_trigger_type));
      break;

    // Other reasons to show a bubble:
    case metrics_util::MANUAL_MANAGE_PASSWORDS:
    case metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION:
    case metrics_util::AUTOMATIC_SIGNIN_TOAST:
      // Do nothing.
      return;

    // Obsolte display dispositions:
    case metrics_util::MANUAL_BLACKLISTED_OBSOLETE:
    case metrics_util::AUTOMATIC_CREDENTIAL_REQUEST_OBSOLETE:
    case metrics_util::NUM_DISPLAY_DISPOSITIONS:
      NOTREACHED();
      return;
  }
}

void PasswordFormMetricsRecorder::RecordUIDismissalReason(
    metrics_util::UIDismissalReason ui_dismissal_reason) {
  if (current_bubble_ != CurrentBubbleOfInterest::kUpdateBubble &&
      current_bubble_ != CurrentBubbleOfInterest::kSaveBubble)
    return;
  auto bubble_dismissal_reason = GetBubbleDismissalReason(ui_dismissal_reason);
  if (bubble_dismissal_reason != BubbleDismissalReason::kUnknown) {
    if (current_bubble_ == CurrentBubbleOfInterest::kUpdateBubble) {
      ukm_entry_builder_.SetUpdating_Prompt_Interaction(
          static_cast<int64_t>(bubble_dismissal_reason));
    } else {
      ukm_entry_builder_.SetSaving_Prompt_Interaction(
          static_cast<int64_t>(bubble_dismissal_reason));
    }
  }

  current_bubble_ = CurrentBubbleOfInterest::kNone;
}

void PasswordFormMetricsRecorder::RecordFillEvent(ManagerAutofillEvent event) {
  ukm_entry_builder_.SetManagerFill_Action(event);
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

// static
bool PasswordFormMetricsRecorder::IsRepeatedUserAction(
    DetailedUserAction action) {
  switch (action) {
    case DetailedUserAction::kUnknown:
      return true;
    case DetailedUserAction::kEditedUsernameInBubble:
    case DetailedUserAction::kSelectedDifferentPasswordInBubble:
    case DetailedUserAction::kCorrectedUsernameInForm:
      return false;
  }
  NOTREACHED();
  return true;
}

}  // namespace password_manager
