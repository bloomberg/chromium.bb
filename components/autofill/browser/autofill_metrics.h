// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_METRICS_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_METRICS_H_

#include <stddef.h>
#include <string>

#include "base/basictypes.h"
#include "components/autofill/browser/autofill_manager_delegate.h"
#include "components/autofill/browser/field_types.h"

namespace base {
class TimeDelta;
}

class AutofillMetrics {
 public:
  // The success or failure of Autocheckout.
  enum AutocheckoutCompletionStatus {
    AUTOCHECKOUT_FAILED,     // The user canceled out of the dialog after
                             // an Autocheckout failure.
    AUTOCHECKOUT_SUCCEEDED,  // The dialog was closed after Autocheckout
                             // succeeded.
  };

  // The action a user took to dismiss a bubble.
  enum BubbleMetric {
    BUBBLE_CREATED = 0,  // The bubble was created.
    BUBBLE_ACCEPTED,     // The user accepted, i.e. confirmed, the bubble.
    BUBBLE_DISMISSED,    // The user dismissed the bubble.
    BUBBLE_IGNORED,      // The user did not interact with the bubble.
    NUM_BUBBLE_METRICS,
  };

  enum DeveloperEngagementMetric {
    // Parsed a form that is potentially autofillable.
    FILLABLE_FORM_PARSED = 0,
    // Parsed a form that is potentially autofillable and contains at least one
    // web developer-specified field type hint, a la
    // http://is.gd/whatwg_autocomplete
    FILLABLE_FORM_CONTAINS_TYPE_HINTS,
    NUM_DEVELOPER_ENGAGEMENT_METRICS,
  };

  // The action the user took to dismiss a dialog.
  enum DialogDismissalAction {
    DIALOG_ACCEPTED = 0,  // The user accepted, i.e. confirmed, the dialog.
    DIALOG_CANCELED,      // The user canceled out of the dialog.
  };

  // The initial state of user that's interacting with a freshly shown
  // requestAutocomplete or Autocheckout dialog.
  enum DialogInitialUserStateMetric {
    // Could not determine the user's state due to failure to communicate with
    // the Wallet server.
    DIALOG_USER_STATE_UNKNOWN = 0,
    // Not signed in, no verified Autofill profiles.
    DIALOG_USER_NOT_SIGNED_IN_NO_AUTOFILL,
    // Not signed in, has verified Autofill profiles.
    DIALOG_USER_NOT_SIGNED_IN_HAS_AUTOFILL,
    // Signed in, no Wallet items, no verified Autofill profiles.
    DIALOG_USER_SIGNED_IN_NO_WALLET_NO_AUTOFILL,
    // Signed in, no Wallet items, has verified Autofill profiles.
    DIALOG_USER_SIGNED_IN_NO_WALLET_HAS_AUTOFILL,
    // Signed in, has Wallet items, no verified Autofill profiles.
    DIALOG_USER_SIGNED_IN_HAS_WALLET_NO_AUTOFILL,
    // Signed in, has Wallet items, has verified Autofill profiles.
    DIALOG_USER_SIGNED_IN_HAS_WALLET_HAS_AUTOFILL,
    NUM_DIALOG_INITIAL_USER_STATE_METRICS
  };

  // Events related to the Autofill popup shown in a requestAutocomplete or
  // Autocheckout dialog.
  enum DialogPopupEvent {
    // An Autofill popup was shown.
    DIALOG_POPUP_SHOWN = 0,
    // The user chose to fill the form with a suggestion from the popup.
    DIALOG_POPUP_FORM_FILLED,
    NUM_DIALOG_POPUP_EVENTS
  };

  // For measuring the frequency of security warnings or errors that can come
  // up as part of the requestAutocomplete flow.
  enum DialogSecurityMetric {
    // Baseline metric: The dialog was shown.
    SECURITY_METRIC_DIALOG_SHOWN = 0,
    // Credit card requested over non-secure protocol.
    SECURITY_METRIC_CREDIT_CARD_OVER_HTTP,
    // Autocomplete data requested from a frame hosted on an origin not matching
    // the main frame's origin.
    SECURITY_METRIC_CROSS_ORIGIN_FRAME,
    NUM_DIALOG_SECURITY_METRICS
  };

  enum InfoBarMetric {
    INFOBAR_SHOWN = 0,  // We showed an infobar, e.g. prompting to save credit
                        // card info.
    INFOBAR_ACCEPTED,   // The user explicitly accepted the infobar.
    INFOBAR_DENIED,     // The user explicitly denied the infobar.
    INFOBAR_IGNORED,    // The user completely ignored the infobar (logged on
                        // tab close).
    NUM_INFO_BAR_METRICS,
  };

  // Metrics measuring how well we predict field types.  Exactly three such
  // metrics are logged for each fillable field in a submitted form: for
  // the heuristic prediction, for the crowd-sourced prediction, and for the
  // overall prediction.
  enum FieldTypeQualityMetric {
    TYPE_UNKNOWN = 0,  // Offered no prediction.
    TYPE_MATCH,        // Predicted correctly.
    TYPE_MISMATCH,     // Predicted incorrectly.
    NUM_FIELD_TYPE_QUALITY_METRICS,
  };

  enum QualityMetric {
    // Logged for each potentially fillable field in a submitted form.
    FIELD_SUBMITTED = 0,

    // A simple successs metric, logged for each field that returns true for
    // |is_autofilled()|.
    FIELD_AUTOFILLED,

    // A simple failure metric, logged for each field that returns false for
    // |is_autofilled()| but has a value that is present in the personal data
    // manager.
    FIELD_NOT_AUTOFILLED,

    // The below are only logged when |FIELD_AUTOFILL_FAILED| is also logged.
    NOT_AUTOFILLED_HEURISTIC_TYPE_UNKNOWN,
    NOT_AUTOFILLED_HEURISTIC_TYPE_MATCH,
    NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
    NOT_AUTOFILLED_SERVER_TYPE_UNKNOWN,
    NOT_AUTOFILLED_SERVER_TYPE_MATCH,
    NOT_AUTOFILLED_SERVER_TYPE_MISMATCH,
    NUM_QUALITY_METRICS,
  };

  // Each of these is logged at most once per query to the server, which in turn
  // occurs at most once per page load.
  enum ServerQueryMetric {
    QUERY_SENT = 0,           // Sent a query to the server.
    QUERY_RESPONSE_RECEIVED,  // Received a response.
    QUERY_RESPONSE_PARSED,    // Successfully parsed the server response.

    // The response was parseable, but provided no improvements relative to our
    // heuristics.
    QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS,

    // Our heuristics detected at least one auto-fillable field, and the server
    // response overrode the type of at least one field.
    QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS,

    // Our heuristics did not detect any auto-fillable fields, but the server
    // response did detect at least one.
    QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS,
    NUM_SERVER_QUERY_METRICS,
  };

  // Each of these metrics is logged only for potentially autofillable forms,
  // i.e. forms with at least three fields, etc.
  // These are used to derive certain "user happiness" metrics.  For example, we
  // can compute the ratio (USER_DID_EDIT_AUTOFILLED_FIELD / USER_DID_AUTOFILL)
  // to see how often users have to correct autofilled data.
  enum UserHappinessMetric {
    // Loaded a page containing forms.
    FORMS_LOADED,
    // Submitted a fillable form -- i.e. one with at least three field values
    // that match the user's stored Autofill data -- and all matching fields
    // were autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_ALL,
    // Submitted a fillable form and some (but not all) matching fields were
    // autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME,
    // Submitted a fillable form and no fields were autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_NONE,
    // Submitted a non-fillable form.
    SUBMITTED_NON_FILLABLE_FORM,

    // User manually filled one of the form fields.
    USER_DID_TYPE,
    // We showed a popup containing Autofill suggestions.
    SUGGESTIONS_SHOWN,
    // Same as above, but only logged once per page load.
    SUGGESTIONS_SHOWN_ONCE,
    // User autofilled at least part of the form.
    USER_DID_AUTOFILL,
    // Same as above, but only logged once per page load.
    USER_DID_AUTOFILL_ONCE,
    // User edited a previously autofilled field.
    USER_DID_EDIT_AUTOFILLED_FIELD,
    // Same as above, but only logged once per page load.
    USER_DID_EDIT_AUTOFILLED_FIELD_ONCE,
    NUM_USER_HAPPINESS_METRICS,
  };

  // For measuring the frequency of errors while communicating with the Wallet
  // server.
  enum WalletErrorMetric {
    // Baseline metric: Issued a request to the Wallet server.
    WALLET_ERROR_BASELINE_ISSUED_REQUEST = 0,
    // A fatal error occured while communicating with the Wallet server. This
    // value has been deprecated.
    WALLET_FATAL_ERROR_DEPRECATED,
    // Received a malformed response from the Wallet server.
    WALLET_MALFORMED_RESPONSE,
    // A network error occured while communicating with the Wallet server.
    WALLET_NETWORK_ERROR,
    // The request was malformed.
    WALLET_BAD_REQUEST,
    // Risk deny, unsupported country, or account closed.
    WALLET_BUYER_ACCOUNT_ERROR,
    // Unknown server side error.
    WALLET_INTERNAL_ERROR,
    // API call had missing or invalid parameters.
    WALLET_INVALID_PARAMS,
    // Online Wallet is down.
    WALLET_SERVICE_UNAVAILABLE,
    // User needs make a cheaper transaction or not use Online Wallet.
    WALLET_SPENDING_LIMIT_EXCEEDED,
    // The server API version of the request is no longer supported.
    WALLET_UNSUPPORTED_API_VERSION,
    // Catch all error type.
    WALLET_UNKNOWN_ERROR,
    NUM_WALLET_ERROR_METRICS
  };

  // For measuring the frequency of "required actions" returned by the Wallet
  // server.  This is similar to the autofill::wallet::RequiredAction enum;
  // but unlike that enum, the values in this one must remain constant over
  // time, so that the metrics can be consistently interpreted on the
  // server-side.
  enum WalletRequiredActionMetric {
    // Baseline metric: Issued a request to the Wallet server.
    WALLET_REQUIRED_ACTION_BASELINE_ISSUED_REQUEST = 0,
    // Values from the autofill::wallet::RequiredAction enum:
    UNKNOWN_TYPE,  // Catch all type.
    GAIA_AUTH,
    PASSIVE_GAIA_AUTH,
    SETUP_WALLET,
    ACCEPT_TOS,
    UPDATE_EXPIRATION_DATE,
    UPGRADE_MIN_ADDRESS,
    CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS,
    VERIFY_CVV,
    INVALID_FORM_FIELD,
    REQUIRE_PHONE_NUMBER,
    NUM_WALLET_REQUIRED_ACTIONS
  };

  AutofillMetrics();
  virtual ~AutofillMetrics();

  // Logs how the user interacted with the Autocheckout bubble.
  virtual void LogAutocheckoutBubbleMetric(BubbleMetric metric) const;

  virtual void LogCreditCardInfoBarMetric(InfoBarMetric metric) const;

  virtual void LogDeveloperEngagementMetric(
      DeveloperEngagementMetric metric) const;

  virtual void LogHeuristicTypePrediction(
      FieldTypeQualityMetric metric,
      AutofillFieldType field_type,
      const std::string& experiment_id) const;
  virtual void LogOverallTypePrediction(
      FieldTypeQualityMetric metric,
      AutofillFieldType field_type,
      const std::string& experiment_id) const;
  virtual void LogServerTypePrediction(FieldTypeQualityMetric metric,
                                       AutofillFieldType field_type,
                                       const std::string& experiment_id) const;

  virtual void LogQualityMetric(QualityMetric metric,
                                const std::string& experiment_id) const;

  virtual void LogServerQueryMetric(ServerQueryMetric metric) const;

  virtual void LogUserHappinessMetric(UserHappinessMetric metric) const;

  // This should be called as soon as the user's signed-in status and Wallet
  // item count is known.  Records that a user starting out in |user_state| is
  // interacting with a dialog of |dialog_type|.
  virtual void LogDialogInitialUserState(
      autofill::DialogType dialog_type,
      DialogInitialUserStateMetric user_type) const;

  // Logs |event| to the popup events histogram for |dialog_type|.
  virtual void LogDialogPopupEvent(autofill::DialogType dialog_type,
                                   DialogPopupEvent event) const;

  // Logs |metric| to the security metrics histogram for |dialog_type|.
  virtual void LogDialogSecurityMetric(autofill::DialogType dialog_type,
                                       DialogSecurityMetric metric) const;

  // This should be called when the requestAutocomplete dialog, invoked by a
  // dialog of type |dialog_type|, is closed.  |duration| should be the time
  // elapsed between the dialog being shown and it being closed.
  // |dismissal_action| should indicate whether the user dismissed the dialog by
  // submitting the form data or by canceling.
  virtual void LogDialogUiDuration(
      const base::TimeDelta& duration,
      autofill::DialogType dialog_type,
      DialogDismissalAction dismissal_action) const;

  // Logs |metric| to the Wallet errors histogram for |dialog_type|.
  virtual void LogWalletErrorMetric(autofill::DialogType dialog_type,
                                    WalletErrorMetric metric) const;

  // Logs |required_action| to the required actions histogram for |dialog_type|.
  virtual void LogWalletRequiredActionMetric(
      autofill::DialogType dialog_type,
      WalletRequiredActionMetric required_action) const;

  virtual void LogAutocheckoutDuration(
      const base::TimeDelta& duration,
      AutocheckoutCompletionStatus status) const;

  // This should be called when a form that has been Autofilled is submitted.
  // |duration| should be the time elapsed between form load and submission.
  virtual void LogFormFillDurationFromLoadWithAutofill(
      const base::TimeDelta& duration) const;

  // This should be called when a fillable form that has not been Autofilled is
  // submitted.  |duration| should be the time elapsed between form load and
  // submission.
  virtual void LogFormFillDurationFromLoadWithoutAutofill(
      const base::TimeDelta& duration) const;

  // This should be called when a form that has been Autofilled is submitted.
  // |duration| should be the time elapsed between the initial form interaction
  // and submission.
  virtual void LogFormFillDurationFromInteractionWithAutofill(
      const base::TimeDelta& duration) const;

  // This should be called when a fillable form that has not been Autofilled is
  // submitted.  |duration| should be the time elapsed between the initial form
  // interaction and submission.
  virtual void LogFormFillDurationFromInteractionWithoutAutofill(
      const base::TimeDelta& duration) const;

  // This should be called each time a page containing forms is loaded.
  virtual void LogIsAutofillEnabledAtPageLoad(bool enabled) const;

  // This should be called each time a new profile is launched.
  virtual void LogIsAutofillEnabledAtStartup(bool enabled) const;

  // This should be called each time a new profile is launched.
  virtual void LogStoredProfileCount(size_t num_profiles) const;

  // Log the number of Autofill suggestions presented to the user when filling a
  // form.
  virtual void LogAddressSuggestionsCount(size_t num_suggestions) const;

  // Logs the experiment id corresponding to a server query response.
  virtual void LogServerExperimentIdForQuery(
      const std::string& experiment_id) const;

  // Logs the experiment id corresponding to an upload to the server.
  virtual void LogServerExperimentIdForUpload(
      const std::string& experiment_id) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillMetrics);
};

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_METRICS_H_
