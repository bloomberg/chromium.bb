// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_FORM_EVENT_LOGGER_H_

#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events.h"

namespace autofill {

class AddressFormEventLogger : public FormEventLoggerBase {
 public:
  AddressFormEventLogger(
      bool is_in_main_frame,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger);

  ~AddressFormEventLogger() override;

  void OnDidFillSuggestion(const AutofillProfile& profile,
                           const FormStructure& form,
                           const AutofillField& field,
                           AutofillSyncSigninState sync_state);

  void OnDidSeeFillableDynamicForm(AutofillSyncSigninState sync_state);

  void OnDidRefill(AutofillSyncSigninState sync_state);

  void OnSubsequentRefillAttempt(AutofillSyncSigninState sync_state);

 protected:
  void RecordPollSuggestions() override;
  void RecordParseForm() override;
  void RecordShowSuggestions() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_FORM_EVENT_LOGGER_H_