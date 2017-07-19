// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/journey_logger.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace payments {

namespace internal {
extern const char kUKMCheckoutEventsEntryName[] =
    "PaymentRequest.CheckoutEvents";
extern const char kUKMCompletionStatusMetricName[] = "CompletionStatus";
extern const char kUKMEventsMetricName[] = "Events";
}  // namespace internal

namespace {

// Returns the JourneyLogger histograms name suffix based on the |section| and
// the |completion_status|.
std::string GetHistogramNameSuffix(
    int section,
    JourneyLogger::CompletionStatus completion_status) {
  std::string name_suffix = "";

  switch (section) {
    case JourneyLogger::SECTION_SHIPPING_ADDRESS:
      name_suffix = "ShippingAddress.";
      break;
    case JourneyLogger::SECTION_CONTACT_INFO:
      name_suffix = "ContactInfo.";
      break;
    case JourneyLogger::SECTION_PAYMENT_METHOD:
      name_suffix = "PaymentMethod.";
      break;
    default:
      break;
  }

  switch (completion_status) {
    case JourneyLogger::COMPLETION_STATUS_COMPLETED:
      name_suffix += "Completed";
      break;
    case JourneyLogger::COMPLETION_STATUS_USER_ABORTED:
      name_suffix += "UserAborted";
      break;
    case JourneyLogger::COMPLETION_STATUS_OTHER_ABORTED:
      name_suffix += "OtherAborted";
      break;
    default:
      break;
  }

  DCHECK(!name_suffix.empty());
  return name_suffix;
}

}  // namespace

JourneyLogger::JourneyLogger(bool is_incognito,
                             const GURL& url,
                             ukm::UkmRecorder* ukm_recorder)
    : is_incognito_(is_incognito),
      events_(EVENT_INITIATED),
      url_(url),
      ukm_recorder_(ukm_recorder) {}

JourneyLogger::~JourneyLogger() {
  if (was_show_called_)
    DCHECK(has_recorded_);
}

void JourneyLogger::IncrementSelectionAdds(Section section) {
  DCHECK_LT(section, SECTION_MAX);
  sections_[section].number_selection_adds_++;
}

void JourneyLogger::IncrementSelectionChanges(Section section) {
  DCHECK_LT(section, SECTION_MAX);
  sections_[section].number_selection_changes_++;
}

void JourneyLogger::IncrementSelectionEdits(Section section) {
  DCHECK_LT(section, SECTION_MAX);
  sections_[section].number_selection_edits_++;
}

void JourneyLogger::SetNumberOfSuggestionsShown(Section section,
                                                int number,
                                                bool has_complete_suggestion) {
  DCHECK_LT(section, SECTION_MAX);
  sections_[section].number_suggestions_shown_ = number;
  sections_[section].is_requested_ = true;
  sections_[section].has_complete_suggestion_ = has_complete_suggestion;
}

void JourneyLogger::SetCanMakePaymentValue(bool value) {
  was_can_make_payments_used_ = true;
  could_make_payment_ |= value;
}

void JourneyLogger::SetShowCalled() {
  was_show_called_ = true;
}

void JourneyLogger::SetEventOccurred(Event event) {
  events_ |= event;
}

void JourneyLogger::SetSelectedPaymentMethod(
    SelectedPaymentMethod payment_method) {
  payment_method_ = payment_method;
}

void JourneyLogger::SetRequestedInformation(bool requested_shipping,
                                            bool requested_email,
                                            bool requested_phone,
                                            bool requested_name) {
  // This method should only be called once per Payment Request.
  DCHECK(requested_information_ == REQUESTED_INFORMATION_MAX);

  requested_information_ =
      (requested_shipping ? REQUESTED_INFORMATION_SHIPPING : 0) |
      (requested_email ? REQUESTED_INFORMATION_EMAIL : 0) |
      (requested_phone ? REQUESTED_INFORMATION_PHONE : 0) |
      (requested_name ? REQUESTED_INFORMATION_NAME : 0);
}

void JourneyLogger::SetCompleted() {
  UMA_HISTOGRAM_BOOLEAN("PaymentRequest.CheckoutFunnel.Completed", true);

  RecordJourneyStatsHistograms(COMPLETION_STATUS_COMPLETED);
}

void JourneyLogger::SetAborted(AbortReason reason) {
  // Don't log abort reasons if the Payment Request was not shown to the user.
  if (was_show_called_) {
    base::UmaHistogramEnumeration("PaymentRequest.CheckoutFunnel.Aborted",
                                  reason, ABORT_REASON_MAX);
  }

  if (reason == ABORT_REASON_ABORTED_BY_USER ||
      reason == ABORT_REASON_USER_NAVIGATION)
    RecordJourneyStatsHistograms(COMPLETION_STATUS_USER_ABORTED);
  else
    RecordJourneyStatsHistograms(COMPLETION_STATUS_OTHER_ABORTED);
}

void JourneyLogger::SetNotShown(NotShownReason reason) {
  base::UmaHistogramEnumeration("PaymentRequest.CheckoutFunnel.NoShow", reason,
                                NOT_SHOWN_REASON_MAX);

  // Record that that Payment Request was initiated here, because nothing else
  // will be recorded for a Payment Request that was not shown to the user.
  UMA_HISTOGRAM_BOOLEAN("PaymentRequest.CheckoutFunnel.Initiated", true);
}

void JourneyLogger::RecordJourneyStatsHistograms(
    CompletionStatus completion_status) {
  DCHECK(!has_recorded_);
  has_recorded_ = true;

  RecordCheckoutFlowMetrics();
  RecordCanMakePaymentStats(completion_status);
  RecordUrlKeyedMetrics(completion_status);

  // These following metrics only make sense if the UI was shown to the user.
  if (was_show_called_) {
    RecordPaymentMethodMetric();
    RecordRequestedInformationMetrics();
    RecordSectionSpecificStats(completion_status);
  }
}

void JourneyLogger::RecordCheckoutFlowMetrics() {
  UMA_HISTOGRAM_BOOLEAN("PaymentRequest.CheckoutFunnel.Initiated", true);

  if (events_ & EVENT_SHOWN)
    UMA_HISTOGRAM_BOOLEAN("PaymentRequest.CheckoutFunnel.Shown", true);

  if (events_ & EVENT_PAY_CLICKED)
    UMA_HISTOGRAM_BOOLEAN("PaymentRequest.CheckoutFunnel.PayClicked", true);

  if (events_ & EVENT_RECEIVED_INSTRUMENT_DETAILS)
    UMA_HISTOGRAM_BOOLEAN(
        "PaymentRequest.CheckoutFunnel.ReceivedInstrumentDetails", true);

  if (events_ & EVENT_SKIPPED_SHOW)
    UMA_HISTOGRAM_BOOLEAN("PaymentRequest.CheckoutFunnel.SkippedShow", true);
}

void JourneyLogger::RecordPaymentMethodMetric() {
  base::UmaHistogramEnumeration("PaymentRequest.SelectedPaymentMethod",
                                payment_method_, SELECTED_PAYMENT_METHOD_MAX);
}

void JourneyLogger::RecordRequestedInformationMetrics() {
  DCHECK(requested_information_ != REQUESTED_INFORMATION_MAX);
  UMA_HISTOGRAM_ENUMERATION("PaymentRequest.RequestedInformation",
                            requested_information_, REQUESTED_INFORMATION_MAX);
}

void JourneyLogger::RecordSectionSpecificStats(
    CompletionStatus completion_status) {
  // Record whether the user had suggestions for each requested information.
  bool user_had_all_requested_information = true;

  // Record whether the user had at least one complete suggestion for each
  // requested section.
  bool user_had_complete_suggestions_ = true;

  for (int i = 0; i < NUMBER_OF_SECTIONS; ++i) {
    std::string name_suffix = GetHistogramNameSuffix(i, completion_status);

    // Only log the metrics for a section if it was requested by the merchant.
    if (sections_[i].is_requested_) {
      base::UmaHistogramCustomCounts(
          "PaymentRequest.NumberOfSelectionAdds." + name_suffix,
          std::min(sections_[i].number_selection_adds_, MAX_EXPECTED_SAMPLE),
          MIN_EXPECTED_SAMPLE, MAX_EXPECTED_SAMPLE, NUMBER_BUCKETS);
      base::UmaHistogramCustomCounts(
          "PaymentRequest.NumberOfSelectionChanges." + name_suffix,
          std::min(sections_[i].number_selection_changes_, MAX_EXPECTED_SAMPLE),
          MIN_EXPECTED_SAMPLE, MAX_EXPECTED_SAMPLE, NUMBER_BUCKETS);
      base::UmaHistogramCustomCounts(
          "PaymentRequest.NumberOfSelectionEdits." + name_suffix,
          std::min(sections_[i].number_selection_edits_, MAX_EXPECTED_SAMPLE),
          MIN_EXPECTED_SAMPLE, MAX_EXPECTED_SAMPLE, NUMBER_BUCKETS);
      base::UmaHistogramCustomCounts(
          "PaymentRequest.NumberOfSuggestionsShown." + name_suffix,
          std::min(sections_[i].number_suggestions_shown_, MAX_EXPECTED_SAMPLE),
          MIN_EXPECTED_SAMPLE, MAX_EXPECTED_SAMPLE, NUMBER_BUCKETS);

      if (sections_[i].number_suggestions_shown_ == 0) {
        user_had_all_requested_information = false;
        user_had_complete_suggestions_ = false;
      } else if (!sections_[i].has_complete_suggestion_) {
        user_had_complete_suggestions_ = false;
      }
    }
  }

  // Record metrics about completion based on whether the user had suggestions
  // for each requested information.
  if (user_had_all_requested_information) {
    base::UmaHistogramEnumeration(
        "PaymentRequest.UserHadSuggestionsForEverything."
        "EffectOnCompletion",
        completion_status, COMPLETION_STATUS_MAX);
  } else {
    base::UmaHistogramEnumeration(
        "PaymentRequest.UserDidNotHaveSuggestionsForEverything."
        "EffectOnCompletion",
        completion_status, COMPLETION_STATUS_MAX);
  }

  // Record metrics about completion based on whether the user had complete
  // suggestions for each requested information.
  if (user_had_complete_suggestions_) {
    base::UmaHistogramEnumeration(
        "PaymentRequest.UserHadCompleteSuggestionsForEverything."
        "EffectOnCompletion",
        completion_status, COMPLETION_STATUS_MAX);
  } else {
    base::UmaHistogramEnumeration(
        "PaymentRequest.UserDidNotHaveCompleteSuggestionsForEverything."
        "EffectOnCompletion",
        completion_status, COMPLETION_STATUS_MAX);
  }
}

void JourneyLogger::RecordCanMakePaymentStats(
    CompletionStatus completion_status) {
  // CanMakePayment always returns true in incognito mode. Don't log the
  // metrics.
  if (is_incognito_)
    return;

  // Record CanMakePayment usage.
  UMA_HISTOGRAM_ENUMERATION("PaymentRequest.CanMakePayment.Usage",
                            was_can_make_payments_used_
                                ? CAN_MAKE_PAYMENT_USED
                                : CAN_MAKE_PAYMENT_NOT_USED,
                            CAN_MAKE_PAYMENT_USE_MAX);

  RecordCanMakePaymentEffectOnShow();
  RecordCanMakePaymentEffectOnCompletion(completion_status);
}

void JourneyLogger::RecordCanMakePaymentEffectOnShow() {
  if (!was_can_make_payments_used_)
    return;

  int effect_on_show = 0;
  if (was_show_called_)
    effect_on_show |= CMP_EFFECT_ON_SHOW_DID_SHOW;
  if (could_make_payment_)
    effect_on_show |= CMP_EFFECT_ON_SHOW_COULD_MAKE_PAYMENT;

  UMA_HISTOGRAM_ENUMERATION("PaymentRequest.CanMakePayment.Used.EffectOnShow",
                            effect_on_show, CMP_EFFECT_ON_SHOW_MAX);
}

void JourneyLogger::RecordCanMakePaymentEffectOnCompletion(
    CompletionStatus completion_status) {
  if (!was_show_called_)
    return;

  std::string histogram_name = "PaymentRequest.CanMakePayment.";
  if (!was_can_make_payments_used_) {
    histogram_name += "NotUsed.WithShowEffectOnCompletion";
  } else if (could_make_payment_) {
    histogram_name += "Used.TrueWithShowEffectOnCompletion";
  } else {
    histogram_name += "Used.FalseWithShowEffectOnCompletion";
  }

  base::UmaHistogramEnumeration(histogram_name, completion_status,
                                COMPLETION_STATUS_MAX);
}

void JourneyLogger::RecordUrlKeyedMetrics(CompletionStatus completion_status) {
  if (!ukm_recorder_ || !url_.is_valid())
    return;

  // Record the Checkout Funnel UKM.
  ukm::SourceId source_id = ukm_recorder_->GetNewSourceID();
  ukm_recorder_->UpdateSourceURL(source_id, url_);
  std::unique_ptr<ukm::UkmEntryBuilder> builder =
      ukm_recorder_->GetEntryBuilder(source_id,
                                     internal::kUKMCheckoutEventsEntryName);
  builder->AddMetric(internal::kUKMCompletionStatusMetricName,
                     completion_status);
  builder->AddMetric(internal::kUKMEventsMetricName, events_);
}

}  // namespace payments
