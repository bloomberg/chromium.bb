// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_form_user_action.h"
#include "components/ukm/public/ukm_recorder.h"

namespace password_manager {

// Internal namespace is intended for component wide access only.
namespace internal {
// UKM Metric names. Exposed in internal namespace for unittesting.

// This metric records whether a submission of a password form has been
// observed. The values 0 and 1 correspond to false and true respectively.
constexpr char kUkmSubmissionObserved[] = "Submission.Observed";

// This metric records the outcome of a password form submission. The values are
// numbered according to PasswordFormMetricsRecorder::SubmitResult.
// Note that no metric is recorded for kSubmitResultNotSubmitted.
constexpr char kUkmSubmissionResult[] = "Submission.SubmissionResult";

// This metric records the classification of a form at submission time. The
// values correspond to PasswordFormMetricsRecorder::SubmittedFormType.
// Note that no metric is recorded for kSubmittedFormTypeUnspecified.
constexpr char kUkmSubmissionFormType[] = "Submission.SubmittedFormType";

}  // namespace internal

class FormFetcher;

// The pupose of this class is to record various types of metrics about the
// behavior of the PasswordFormManager and its interaction with the user and the
// page.
class PasswordFormMetricsRecorder {
 public:
  // |ukm_entry_builder| is the destination into which UKM metrics are recorded.
  // It may be nullptr, in which case no UKM metrics are recorded. This should
  // be created via the static CreateUkmEntryBuilder() method of this class.
  PasswordFormMetricsRecorder(
      bool is_main_frame_secure,
      std::unique_ptr<ukm::UkmEntryBuilder> ukm_entry_builder);
  ~PasswordFormMetricsRecorder();

  // Creates a UkmEntryBuilder that can be used to record metrics into the event
  // "PasswordForm". |source_id| should be bound the the correct URL in the
  // |ukm_recorder| when this function is called.
  static std::unique_ptr<ukm::UkmEntryBuilder> CreateUkmEntryBuilder(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id);

  // ManagerAction - What does the PasswordFormManager do with this form? Either
  // it fills it, or it doesn't. If it doesn't fill it, that's either
  // because it has no match or it is disabled via the AUTOCOMPLETE=off
  // attribute. Note that if we don't have an exact match, we still provide
  // candidates that the user may end up choosing.
  enum ManagerAction {
    kManagerActionNone = 0,
    kManagerActionAutofilled,
    kManagerActionBlacklisted_Obsolete,
    kManagerActionMax
  };

  // Same as above without the obsoleted 'Blacklisted' action.
  enum ManagerActionNew {
    kManagerActionNewNone = 0,
    kManagerActionNewAutofilled,
    kManagerActionNewMax
  };

  // Result - What happens to the form?
  enum SubmitResult {
    kSubmitResultNotSubmitted = 0,
    kSubmitResultFailed,
    kSubmitResultPassed,
    kSubmitResultMax
  };

  // Enumerates whether there were `suppressed` credentials. These are stored
  // credentials that were not filled, even though they might be related to the
  // observed form. See FormFetcher::GetSuppressed* for details.
  //
  // If suppressed credentials exist, it is also recorded whether their username
  // and/or password matched those submitted.
  enum SuppressedAccountExistence {
    kSuppressedAccountNone,
    // Recorded when there exists a suppressed account, but there was no
    // submitted form to compare its username and password to.
    kSuppressedAccountExists,
    // Recorded when there was a submitted form.
    kSuppressedAccountExistsDifferentUsername,
    kSuppressedAccountExistsSameUsername,
    kSuppressedAccountExistsSameUsernameAndPassword,
    kSuppressedAccountExistenceMax,
  };

  // What the form is used for. kSubmittedFormTypeUnspecified is only set before
  // the SetSubmittedFormType() is called, and should never be actually
  // uploaded.
  enum SubmittedFormType {
    kSubmittedFormTypeLogin,
    kSubmittedFormTypeLoginNoUsername,
    kSubmittedFormTypeChangePasswordEnabled,
    kSubmittedFormTypeChangePasswordDisabled,
    kSubmittedFormTypeChangePasswordNoUsername,
    kSubmittedFormTypeSignup,
    kSubmittedFormTypeSignupNoUsername,
    kSubmittedFormTypeLoginAndSignup,
    kSubmittedFormTypeUnspecified,
    kSubmittedFormTypeMax
  };

  // The maximum number of combinations of the ManagerAction, UserAction and
  // SubmitResult enums.
  // This is used when recording the actions taken by the form in UMA.
  static constexpr int kMaxNumActionsTaken =
      kManagerActionMax * static_cast<int>(UserAction::kMax) * kSubmitResultMax;

  // Same as above but for ManagerActionNew instead of ManagerAction.
  static constexpr int kMaxNumActionsTakenNew =
      kManagerActionNewMax * static_cast<int>(UserAction::kMax) *
      kSubmitResultMax;

  // The maximum number of combinations recorded into histograms in the
  // PasswordManager.SuppressedAccount.* family.
  static constexpr int kMaxSuppressedAccountStats =
      kSuppressedAccountExistenceMax *
      PasswordFormMetricsRecorder::kManagerActionNewMax *
      static_cast<int>(UserAction::kMax) *
      PasswordFormMetricsRecorder::kSubmitResultMax;

  // Called if the user could generate a password for this form.
  void MarkGenerationAvailable();

  // Stores whether the form has had its password auto generated by the browser.
  void SetHasGeneratedPassword(bool has_generated_password);

  // Stores the password manager and user actions and logs them.
  void SetManagerAction(ManagerAction manager_action);
  void SetUserAction(UserAction user_action);

  // Call these if/when we know the form submission worked or failed.
  // These routines are used to update internal statistics ("ActionsTaken").
  void LogSubmitPassed();
  void LogSubmitFailed();

  // Call this once the submitted form type has been determined.
  void SetSubmittedFormType(SubmittedFormType form_type);

  // Records all histograms in the PasswordManager.SuppressedAccount.* family.
  // Takes the FormFetcher intance which owns the login data from PasswordStore.
  // |pending_credentials| stores credentials when the form was submitted but
  // success was still unknown. It contains credentials that are ready to be
  // written (saved or updated) to a password store.
  void RecordHistogramsOnSuppressedAccounts(
      bool observed_form_origin_has_cryptographic_scheme,
      const FormFetcher& form_fetcher,
      const autofill::PasswordForm& pending_credentials);

  // Converts the "ActionsTaken" fields (using ManagerActionNew) into an int so
  // they can be logged to UMA.
  // Public for testing.
  int GetActionsTakenNew() const;

 private:
  // Converts the "ActionsTaken" fields into an int so they can be logged to
  // UMA.
  int GetActionsTaken() const;

  // When supplied with the list of all |suppressed_forms| that belong to
  // certain suppressed credential type (see FormFetcher::GetSuppressed*),
  // filters that list down to forms whose type matches |manual_or_generated|,
  // and selects the suppressed account that matches |pending_credentials| most
  // closely. |pending_credentials| stores credentials when the form was
  // submitted but success was still unknown. It contains credentials that are
  // ready to be written (saved or updated) to a password store.
  SuppressedAccountExistence GetBestMatchingSuppressedAccount(
      const std::vector<const autofill::PasswordForm*>& suppressed_forms,
      autofill::PasswordForm::Type manual_or_generated,
      const autofill::PasswordForm& pending_credentials) const;

  // Encodes a UMA histogram sample for |best_matching_account| and
  // GetActionsTakenNew(). This is a mixed-based representation of a combination
  // of four attributes:
  //  -- whether there were suppressed credentials (and if so, their relation to
  //     the submitted username/password).
  //  -- whether the |observed_form_| got ultimately submitted
  //  -- what action the password manager performed (|manager_action_|),
  //  -- and what action the user performed (|user_action_|_).
  int GetHistogramSampleForSuppressedAccounts(
      SuppressedAccountExistence best_matching_account) const;

  // Records a metric into |ukm_entry_builder_| if it is not nullptr.
  void RecordUkmMetric(const char* metric_name, int64_t value);

  // True if the main frame's visible URL, at the time this PasswordFormManager
  // was created, is secure.
  const bool is_main_frame_secure_;

  // Whether the user can choose to generate a password for this form.
  bool generation_available_ = false;

  // Whether this form has an auto generated password.
  bool has_generated_password_ = false;

  // These three fields record the "ActionsTaken" by the browser and
  // the user with this form, and the result. They are combined and
  // recorded in UMA when the PasswordFormMetricsRecorder is destroyed.
  ManagerAction manager_action_ = kManagerActionNone;
  UserAction user_action_ = UserAction::kNone;
  SubmitResult submit_result_ = kSubmitResultNotSubmitted;

  // Form type of the form that the PasswordFormManager is managing. Set after
  // submission as the classification of the form can change depending on what
  // data the user has entered.
  SubmittedFormType submitted_form_type_ = kSubmittedFormTypeUnspecified;

  // Records URL keyed metrics (UKMs) and submits them on its destruction. May
  // be a nullptr in which case no recording is expected.
  std::unique_ptr<ukm::UkmEntryBuilder> ukm_entry_builder_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormMetricsRecorder);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_
