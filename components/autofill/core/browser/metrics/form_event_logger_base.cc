// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_event_logger_base.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/form_events.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures_util.h"

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
    FormSignature form_signature,
    AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;
  if (!has_logged_interacted_) {
    has_logged_interacted_ = true;
    LogUkmInteractedWithForm(form_signature);
    Log(FORM_EVENT_INTERACTED_ONCE);
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

void FormEventLoggerBase::OnDidParseForm() {
  Log(FORM_EVENT_DID_PARSE_FORM);
  RecordParseForm();
}

void FormEventLoggerBase::OnPopupSuppressed(const FormStructure& form,
                                            const AutofillField& field) {
  Log(FORM_EVENT_POPUP_SUPPRESSED);
  if (!has_logged_popup_suppressed_) {
    has_logged_popup_suppressed_ = true;
    Log(FORM_EVENT_POPUP_SUPPRESSED_ONCE);
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

  Log(FORM_EVENT_SUGGESTIONS_SHOWN);
  if (!has_logged_suggestions_shown_) {
    has_logged_suggestions_shown_ = true;
    Log(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE);
    OnSuggestionsShownOnce();
  }

  RecordShowSuggestions();
}

void FormEventLoggerBase::OnWillSubmitForm(AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;
  // Not logging this kind of form if we haven't logged a user interaction.
  if (!has_logged_interacted_)
    return;

  // Not logging twice.
  if (has_logged_will_submit_)
    return;
  has_logged_will_submit_ = true;

  LogWillSubmitForm();

  if (has_logged_suggestions_shown_) {
    Log(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE);
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

  LogFormSubmitted();

  if (has_logged_suggestions_shown_ || force_logging) {
    Log(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE);
    OnSuggestionsShownSubmittedOnce(form);
  }
}

void FormEventLoggerBase::Log(FormEvent event) const {
  DCHECK_LT(event, NUM_FORM_EVENTS);
  std::string name("Autofill.FormEvents." + form_type_name_);
  base::UmaHistogramEnumeration(name, event, NUM_FORM_EVENTS);

  // Log again in a different histogram so that iframes can be analyzed on
  // their own.
  base::UmaHistogramEnumeration(
      name + (is_in_main_frame_ ? ".IsInMainFrame" : ".IsInIFrame"), event,
      NUM_FORM_EVENTS);

  // Allow specialized type of logging.
  OnLog(name, event);

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

void FormEventLoggerBase::LogWillSubmitForm() {
  if (!has_logged_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE);
  }
}

void FormEventLoggerBase::LogFormSubmitted() {
  if (!has_logged_suggestion_filled_) {
    Log(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE);
  }
}

void FormEventLoggerBase::LogUkmInteractedWithForm(
    FormSignature form_signature) {
  form_interactions_ukm_logger_->LogInteractedWithForm(
      /*is_for_credit_card=*/false, local_record_type_count_,
      server_record_type_count_, form_signature);
}

}  // namespace autofill
