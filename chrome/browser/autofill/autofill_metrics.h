// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_

#include <stddef.h>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/autofill/autofill_manager_delegate.h"
#include "chrome/browser/autofill/field_types.h"

namespace base {
class TimeDelta;
}

class AutofillMetrics {
 public:
  // The success or failure of Autocheckout.
  enum AutocheckoutCompletionStatus {
    AUTOCHECKOUT_FAILED,    // The user canceled out of the dialog after
                            // an Autocheckout failure.
    AUTOCHECKOUT_SUCCEEDED  // The dialog was closed after Autocheckout
                            // succeeded.
  };

  enum DeveloperEngagementMetric {
    // Parsed a form that is potentially autofillable.
    FILLABLE_FORM_PARSED = 0,
    // Parsed a form that is potentially autofillable and contains at least one
    // web developer-specified field type hint, a la
    // http://is.gd/whatwg_autocomplete
    FILLABLE_FORM_CONTAINS_TYPE_HINTS,
    NUM_DEVELOPER_ENGAGEMENT_METRICS
  };

  // The action the user took to dismiss a dialog.
  enum DialogDismissalAction {
    DIALOG_ACCEPTED = 0,  // The user accepted, i.e. confirmed, the dialog.
    DIALOG_CANCELED       // The user canceled out of the dialog.
  };

  enum InfoBarMetric {
    INFOBAR_SHOWN = 0,  // We showed an infobar, e.g. prompting to save credit
                        // card info.
    INFOBAR_ACCEPTED,   // The user explicitly accepted the infobar.
    INFOBAR_DENIED,     // The user explicitly denied the infobar.
    INFOBAR_IGNORED,    // The user completely ignored the infobar (logged on
                        // tab close).
    NUM_INFO_BAR_METRICS
  };

  // Metrics measuring how well we predict field types.  Exactly three such
  // metrics are logged for each fillable field in a submitted form: for
  // the heuristic prediction, for the crowd-sourced prediction, and for the
  // overall prediction.
  enum FieldTypeQualityMetric {
    TYPE_UNKNOWN = 0,  // Offered no prediction.
    TYPE_MATCH,        // Predicted correctly.
    TYPE_MISMATCH,     // Predicted incorrectly.
    NUM_FIELD_TYPE_QUALITY_METRICS
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
    NUM_QUALITY_METRICS
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
    NUM_SERVER_QUERY_METRICS
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
    NUM_USER_HAPPINESS_METRICS
  };

  AutofillMetrics();
  virtual ~AutofillMetrics();

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

  // This should be called when the requestAutocomplete dialog, invoked by the
  // |requester|, is closed.  |duration| should be the time elapsed between the
  // dialog being shown and it being closed.  |dismissal_action| should indicate
  // whether the user dismissed the dialog by submitting the form data or by
  // cancelling.
  virtual void LogRequestAutocompleteUiDuration(
      const base::TimeDelta& duration,
      autofill::DialogType dialog_type,
      DialogDismissalAction dismissal_action) const;

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

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
