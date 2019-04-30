// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_event_logger_base.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"

namespace autofill {

FormEventLoggerBase::FormEventLoggerBase(
    const std::string& form_type_name,
    bool is_in_main_frame,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger)
    : form_type_name_(form_type_name),
      is_in_main_frame_(is_in_main_frame),
      form_interactions_ukm_logger_(form_interactions_ukm_logger) {}

FormEventLoggerBase::~FormEventLoggerBase() = default;

void FormEventLoggerBase::OnDidInteractWithAutofillableForm(
    const FormStructure& form,
    AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;
  if (!has_logged_interacted_) {
    has_logged_interacted_ = true;
    LogUkmInteractedWithForm(form.form_signature());
    Log(FORM_EVENT_INTERACTED_ONCE, form);
  }
}

void FormEventLoggerBase::OnDidPollSuggestions(
    const FormFieldData& field,
    AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;
  // Record only one poll user action for consecutive polls of the same field.
  // This is to avoid recording too many poll actions (for example when a user
  // types in a field, triggering multiple queries) to make the analysis more
  // simple.
  if (!field.SameFieldAs(last_polled_field_)) {
    RecordPollSuggestions();
    last_polled_field_ = field;
  }
}

void FormEventLoggerBase::OnDidParseForm(const FormStructure& form) {
  Log(FORM_EVENT_DID_PARSE_FORM, form);
  RecordParseForm();
}

void FormEventLoggerBase::OnPopupSuppressed(const FormStructure& form,
                                            const AutofillField& field) {
  Log(FORM_EVENT_POPUP_SUPPRESSED, form);
  if (!has_logged_popup_suppressed_) {
    has_logged_popup_suppressed_ = true;
    Log(FORM_EVENT_POPUP_SUPPRESSED_ONCE, form);
  }
}

void FormEventLoggerBase::OnDidShowSuggestions(
    const FormStructure& form,
    const AutofillField& field,
    const base::TimeTicks& form_parsed_timestamp,
    AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;
  form_interactions_ukm_logger_->LogSuggestionsShown(form, field,
                                                     form_parsed_timestamp);

  Log(FORM_EVENT_SUGGESTIONS_SHOWN, form);
  if (!has_logged_suggestions_shown_) {
    has_logged_suggestions_shown_ = true;
    Log(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, form);
    OnSuggestionsShownOnce();
  }

  RecordShowSuggestions();
}

void FormEventLoggerBase::OnWillSubmitForm(AutofillSyncSigninState sync_state,
                                           const FormStructure& form) {
  sync_state_ = sync_state;
  // Not logging this kind of form if we haven't logged a user interaction.
  if (!has_logged_interacted_)
    return;

  // Not logging twice.
  if (has_logged_will_submit_)
    return;
  has_logged_will_submit_ = true;

  LogWillSubmitForm(form);

  if (has_logged_suggestions_shown_) {
    Log(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, form);
  }

  base::RecordAction(base::UserMetricsAction("Autofill_OnWillSubmitForm"));
}

void FormEventLoggerBase::OnFormSubmitted(bool force_logging,
                                          AutofillSyncSigninState sync_state,
                                          const FormStructure& form) {
  sync_state_ = sync_state;
  // Not logging this kind of form if we haven't logged a user interaction.
  if (!has_logged_interacted_)
    return;

  // Not logging twice.
  if (has_logged_submitted_)
    return;
  has_logged_submitted_ = true;

  LogFormSubmitted(form);

  if (has_logged_suggestions_shown_ || force_logging) {
    Log(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, form);
    OnSuggestionsShownSubmittedOnce(form);
  }
}

void FormEventLoggerBase::Log(FormEvent event,
                              const FormStructure& form) const {
  DCHECK_LT(event, NUM_FORM_EVENTS);
  std::string name("Autofill.FormEvents." + form_type_name_);
  base::UmaHistogramEnumeration(name, event, NUM_FORM_EVENTS);

  // Log UKM metrics for only autofillable form events.
  if (form.IsAutofillable()) {
    form_interactions_ukm_logger_->LogFormEvent(event, form.GetFormTypes(),
                                                form.form_parsed_timestamp());
  }

  // Log again in a different histogram so that iframes can be analyzed on
  // their own.
  base::UmaHistogramEnumeration(
      name + (is_in_main_frame_ ? ".IsInMainFrame" : ".IsInIFrame"), event,
      NUM_FORM_EVENTS);

  // Allow specialized types of logging, e.g. splitting metrics in useful ways.
  OnLog(name, event, form);

  // Logging again in a different histogram for segmentation purposes.
  if (server_record_type_count_ == 0 && local_record_type_count_ == 0)
    name += ".WithNoData";
  else if (server_record_type_count_ > 0 && local_record_type_count_ == 0)
    name += ".WithOnlyServerData";
  else if (server_record_type_count_ == 0 && local_record_type_count_ > 0)
    name += ".WithOnlyLocalData";
  else
    name += ".WithBothServerAndLocalData";
  base::UmaHistogramEnumeration(name, event, NUM_FORM_EVENTS);
  base::UmaHistogramEnumeration(
      name + AutofillMetrics::GetMetricsSyncStateSuffix(sync_state_), event,
      NUM_FORM_EVENTS);
}

void FormEventLoggerBase::LogWillSubmitForm(const FormStructure& form) {
  if (!has_logged_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, form);
  }
}

void FormEventLoggerBase::LogFormSubmitted(const FormStructure& form) {
  if (!has_logged_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, form);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, form);
  }
}

void FormEventLoggerBase::LogUkmInteractedWithForm(
    FormSignature form_signature) {
  form_interactions_ukm_logger_->LogInteractedWithForm(
      /*is_for_credit_card=*/false, local_record_type_count_,
      server_record_type_count_, form_signature);
}

}  // namespace autofill
