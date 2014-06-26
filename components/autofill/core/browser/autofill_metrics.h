// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_

#include <stddef.h>
#include <string>

#include "base/basictypes.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/field_types.h"

namespace base {
class TimeDelta;
}

namespace autofill {

class AutofillMetrics {
 public:
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
    DIALOG_ACCEPTED = 0,  // The user accepted, i.e. submitted, the dialog.
    DIALOG_CANCELED,      // The user canceled out of the dialog.
  };

  // The state of the Autofill dialog when it was dismissed.
  enum DialogDismissalState {
    // The user submitted with no data available to save.
    DEPRECATED_DIALOG_ACCEPTED_EXISTING_DATA,
    // The saved details to Online Wallet on submit.
    DIALOG_ACCEPTED_SAVE_TO_WALLET,
    // The saved details to the local Autofill database on submit.
    DIALOG_ACCEPTED_SAVE_TO_AUTOFILL,
    // The user submitted without saving any edited sections.
    DIALOG_ACCEPTED_NO_SAVE,
    // The user canceled with no edit UI showing.
    DIALOG_CANCELED_NO_EDITS,
    // The user canceled with edit UI showing, but no invalid fields.
    DIALOG_CANCELED_NO_INVALID_FIELDS,
    // The user canceled with at least one invalid field.
    DIALOG_CANCELED_WITH_INVALID_FIELDS,
    // The user canceled while the sign-in form was showing.
    DIALOG_CANCELED_DURING_SIGNIN,
    // The user submitted using data already stored in Wallet.
    DIALOG_ACCEPTED_EXISTING_WALLET_DATA,
    // The user submitted using data already stored in Autofill.
    DIALOG_ACCEPTED_EXISTING_AUTOFILL_DATA,
    NUM_DIALOG_DISMISSAL_STATES
  };

  // The initial state of user that's interacting with a freshly shown Autofill
  // dialog.
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

  // Events related to the Autofill popup shown in a requestAutocomplete
  // dialog.
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

  // For measuring how users are interacting with the Autofill dialog UI.
  enum DialogUiEvent {
    // Baseline metric: The dialog was shown.
    DIALOG_UI_SHOWN = 0,

    // Dialog dismissal actions:
    DIALOG_UI_ACCEPTED,
    DIALOG_UI_CANCELED,

    // Selections within the account switcher:
    // Switched from a Wallet account to local Autofill data.
    DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_TO_AUTOFILL,
    // Switched from local Autofill data to a Wallet account.
    DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_TO_WALLET,
    // Switched from one Wallet account to another one.
    DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_WALLET_ACCOUNT,

    // The sign-in UI was shown.
    DIALOG_UI_SIGNIN_SHOWN,

    // Selecting a different item from a suggestion menu dropdown:
    DEPRECATED_DIALOG_UI_EMAIL_SELECTED_SUGGESTION_CHANGED,
    DIALOG_UI_BILLING_SELECTED_SUGGESTION_CHANGED,
    DIALOG_UI_CC_BILLING_SELECTED_SUGGESTION_CHANGED,
    DIALOG_UI_SHIPPING_SELECTED_SUGGESTION_CHANGED,
    DIALOG_UI_CC_SELECTED_SUGGESTION_CHANGED,

    // Showing the editing UI for a section of the dialog:
    DEPRECATED_DIALOG_UI_EMAIL_EDIT_UI_SHOWN,
    DEPRECATED_DIALOG_UI_BILLING_EDIT_UI_SHOWN,
    DEPRECATED_DIALOG_UI_CC_BILLING_EDIT_UI_SHOWN,
    DEPRECATED_DIALOG_UI_SHIPPING_EDIT_UI_SHOWN,
    DEPRECATED_DIALOG_UI_CC_EDIT_UI_SHOWN,

    // Adding a new item in a section of the dialog:
    DEPRECATED_DIALOG_UI_EMAIL_ITEM_ADDED,
    DIALOG_UI_BILLING_ITEM_ADDED,
    DIALOG_UI_CC_BILLING_ITEM_ADDED,
    DIALOG_UI_SHIPPING_ITEM_ADDED,
    DIALOG_UI_CC_ITEM_ADDED,

    // Also an account switcher menu item. The user selected the
    // "add account" option.
    DIALOG_UI_ACCOUNT_CHOOSER_TRIED_TO_ADD_ACCOUNT,

    NUM_DIALOG_UI_EVENTS
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

  // For measuring the network request time of various Wallet API calls. See
  // WalletClient::RequestType.
  enum WalletApiCallMetric {
    UNKNOWN_API_CALL,  // Catch all. Should never be used.
    ACCEPT_LEGAL_DOCUMENTS,
    AUTHENTICATE_INSTRUMENT,
    GET_FULL_WALLET,
    GET_WALLET_ITEMS,
    SAVE_TO_WALLET,
    NUM_WALLET_API_CALLS
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
    // The merchant has been blacklisted for Online Wallet due to some manner
    // of compliance violation.
    WALLET_UNSUPPORTED_MERCHANT,
    // Buyer Legal Address has a country which is unsupported by Wallet.
    WALLET_BUYER_LEGAL_ADDRESS_NOT_SUPPORTED,
    // Wallet's Know Your Customer(KYC) action is pending/failed for this user.
    WALLET_UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS,
    // Chrome version is unsupported or provided API key not allowed.
    WALLET_UNSUPPORTED_USER_AGENT_OR_API_KEY,
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
    UNKNOWN_REQUIRED_ACTION,  // Catch all type.
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

  virtual void LogCreditCardInfoBarMetric(InfoBarMetric metric) const;

  virtual void LogDeveloperEngagementMetric(
      DeveloperEngagementMetric metric) const;

  virtual void LogHeuristicTypePrediction(FieldTypeQualityMetric metric,
                                          ServerFieldType field_type) const;
  virtual void LogOverallTypePrediction(FieldTypeQualityMetric metric,
                                        ServerFieldType field_type) const;
  virtual void LogServerTypePrediction(FieldTypeQualityMetric metric,
                                       ServerFieldType field_type) const;

  virtual void LogServerQueryMetric(ServerQueryMetric metric) const;

  virtual void LogUserHappinessMetric(UserHappinessMetric metric) const;

  // Logs |state| to the dismissal states histogram.
  virtual void LogDialogDismissalState(DialogDismissalState state) const;

  // This should be called as soon as the user's signed-in status and Wallet
  // item count is known.  Records that a user starting out in |user_state| is
  // interacting with a dialog.
  virtual void LogDialogInitialUserState(
      DialogInitialUserStateMetric user_type) const;

  // Logs the time elapsed between the dialog being shown and when it is ready
  // for user interaction.
  virtual void LogDialogLatencyToShow(const base::TimeDelta& duration) const;

  // Logs |event| to the popup events histogram.
  virtual void LogDialogPopupEvent(DialogPopupEvent event) const;

  // Logs |metric| to the security metrics histogram.
  virtual void LogDialogSecurityMetric(DialogSecurityMetric metric) const;

  // This should be called when the Autofill dialog is closed.  |duration|
  // should be the time elapsed between the dialog being shown and it being
  // closed.  |dismissal_action| should indicate whether the user dismissed
  // the dialog by submitting the form data or by canceling.
  virtual void LogDialogUiDuration(
      const base::TimeDelta& duration,
      DialogDismissalAction dismissal_action) const;

  // Logs |event| to the UI events histogram.
  virtual void LogDialogUiEvent(DialogUiEvent event) const;

  // Logs |metric| to the Wallet errors histogram.
  virtual void LogWalletErrorMetric(WalletErrorMetric metric) const;

  // Logs the network request time of Wallet API calls.
  virtual void LogWalletApiCallDuration(
      WalletApiCallMetric metric,
      const base::TimeDelta& duration) const;

  // Logs that the Wallet API call corresponding to |metric| was malformed.
  virtual void LogWalletMalformedResponseMetric(
      WalletApiCallMetric metric) const;

  // Logs |required_action| to the required actions histogram.
  virtual void LogWalletRequiredActionMetric(
      WalletRequiredActionMetric required_action) const;

  // Logs HTTP response codes recieved by wallet client.
  virtual void LogWalletResponseCode(int response_code) const;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillMetrics);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_
