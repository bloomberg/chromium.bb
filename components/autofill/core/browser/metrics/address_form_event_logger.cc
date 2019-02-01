// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/address_form_event_logger.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/metrics/form_events.h"

namespace autofill {

AddressFormEventLogger::AddressFormEventLogger(
    bool is_in_main_frame,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger)
    : FormEventLoggerBase("Address",
                          is_in_main_frame,
                          form_interactions_ukm_logger) {}

AddressFormEventLogger::~AddressFormEventLogger() = default;

void AddressFormEventLogger::OnDidFillSuggestion(
    const AutofillProfile& profile,
    const FormStructure& form,
    const AutofillField& field,
    AutofillSyncSigninState sync_state) {
  AutofillProfile::RecordType record_type = profile.record_type();
  sync_state_ = sync_state;

  form_interactions_ukm_logger_->LogDidFillSuggestion(
      record_type,
      /*is_for_for_credit_card=*/false, form, field);

  if (record_type == AutofillProfile::SERVER_PROFILE) {
    Log(FORM_EVENT_SERVER_SUGGESTION_FILLED);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED);
  }

  if (!has_logged_suggestion_filled_) {
    has_logged_suggestion_filled_ = true;
    logged_suggestion_filled_was_server_data_ =
        record_type == AutofillProfile::SERVER_PROFILE;
    Log(record_type == AutofillProfile::SERVER_PROFILE
            ? FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE
            : FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE);
  }

  base::RecordAction(
      base::UserMetricsAction("Autofill_FilledProfileSuggestion"));
}

void AddressFormEventLogger::OnDidSeeFillableDynamicForm(
    AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;
  Log(FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM);
}

void AddressFormEventLogger::OnDidRefill(AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;
  Log(FORM_EVENT_DID_DYNAMIC_REFILL);
}

void AddressFormEventLogger::OnSubsequentRefillAttempt(
    AutofillSyncSigninState sync_state) {
  sync_state_ = sync_state;
  Log(FORM_EVENT_DYNAMIC_CHANGE_AFTER_REFILL);
}

void AddressFormEventLogger::RecordPollSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_PolledProfileSuggestions"));
}

void AddressFormEventLogger::RecordParseForm() {
  base::RecordAction(base::UserMetricsAction("Autofill_ParsedProfileForm"));
}

void AddressFormEventLogger::RecordShowSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedProfileSuggestions"));
}

}  // namespace autofill
