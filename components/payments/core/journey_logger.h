// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_
#define COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_

#include <string>

#include "base/macros.h"
#include "url/gurl.h"

namespace ukm {
class UkmRecorder;
}

namespace payments {

namespace internal {
// Name constants are exposed here so they can be referenced from tests.
extern const char kUKMCheckoutEventsEntryName[];
extern const char kUKMCompletionStatusMetricName[];
extern const char kUKMEventsMetricName[];
}  // namespace internal

// A class to keep track of different stats during a Payment Request journey. It
// collects different metrics during the course of the checkout flow, like the
// number of credit cards that the user added or edited. The metrics will be
// logged when RecordJourneyStatsHistograms is called with the completion status
// of the Payment Request.
class JourneyLogger {
 public:
  // Note: Java counterparts will be generated for these enums.

  // The different sections of a Payment Request. Used to record journey
  // stats.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: Section
  enum Section {
    SECTION_CONTACT_INFO = 0,
    SECTION_PAYMENT_METHOD = 1,
    SECTION_SHIPPING_ADDRESS = 2,
    SECTION_MAX,
  };

  // For the CanMakePayment histograms.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: CanMakePaymentUsage
  enum CanMakePaymentUsage {
    CAN_MAKE_PAYMENT_USED = 0,
    CAN_MAKE_PAYMENT_NOT_USED = 1,
    CAN_MAKE_PAYMENT_USE_MAX,
  };

  // The information requested by the merchant.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: RequestedInformation
  enum RequestedInformation {
    REQUESTED_INFORMATION_NONE = 0,
    REQUESTED_INFORMATION_EMAIL = 1 << 0,
    REQUESTED_INFORMATION_PHONE = 1 << 1,
    REQUESTED_INFORMATION_SHIPPING = 1 << 2,
    REQUESTED_INFORMATION_NAME = 1 << 3,
    REQUESTED_INFORMATION_MAX = 16,
  };

  // The payment method that was used by the user to complete the transaction.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: SelectedPaymentMethod
  enum SelectedPaymentMethod {
    SELECTED_PAYMENT_METHOD_CREDIT_CARD = 0,
    SELECTED_PAYMENT_METHOD_ANDROID_PAY = 1,
    SELECTED_PAYMENT_METHOD_OTHER_PAYMENT_APP = 2,
    SELECTED_PAYMENT_METHOD_MAX = 3,
  };

  // Used to mesure the impact of the CanMakePayment return value on whether the
  // Payment Request is shown to the user.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: CanMakePaymentEffectOnShow
  enum CmpEffectOnShow {
    CMP_EFFECT_ON_SHOW_COULD_NOT_MAKE_PAYMENT_AND_DID_NOT_SHOW = 0,
    CMP_EFFECT_ON_SHOW_DID_SHOW = 1 << 0,
    CMP_EFFECT_ON_SHOW_COULD_MAKE_PAYMENT = 1 << 1,
    CMP_EFFECT_ON_SHOW_MAX = 4,
  };

  // Used to log different parameters' effect on whether the transaction was
  // completed.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: CompletionStatus
  enum CompletionStatus {
    COMPLETION_STATUS_COMPLETED = 0,
    COMPLETION_STATUS_USER_ABORTED = 1,
    COMPLETION_STATUS_OTHER_ABORTED = 2,
    COMPLETION_STATUS_MAX,
  };

  // Used to record the different events that happened during the Payment
  // Request.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: Event
  enum Event {
    EVENT_INITIATED = 0,
    EVENT_SHOWN = 1 << 0,
    EVENT_PAY_CLICKED = 1 << 1,
    EVENT_RECEIVED_INSTRUMENT_DETAILS = 1 << 2,
    EVENT_SKIPPED_SHOW = 1 << 3,
    EVENT_ENUM_MAX = 16,
  };

  // The reason why the Payment Request was aborted.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AbortReason
  enum AbortReason {
    ABORT_REASON_ABORTED_BY_USER = 0,
    ABORT_REASON_ABORTED_BY_MERCHANT = 1,
    ABORT_REASON_INVALID_DATA_FROM_RENDERER = 2,
    ABORT_REASON_MOJO_CONNECTION_ERROR = 3,
    ABORT_REASON_MOJO_RENDERER_CLOSING = 4,
    ABORT_REASON_INSTRUMENT_DETAILS_ERROR = 5,
    ABORT_REASON_NO_MATCHING_PAYMENT_METHOD = 6,   // Deprecated.
    ABORT_REASON_NO_SUPPORTED_PAYMENT_METHOD = 7,  // Deprecated.
    ABORT_REASON_OTHER = 8,
    ABORT_REASON_USER_NAVIGATION = 9,
    ABORT_REASON_MERCHANT_NAVIGATION = 10,
    ABORT_REASON_MAX,
  };

  // The reason why the Payment Request was not shown to the user.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: NotShownReason
  enum NotShownReason {
    NOT_SHOWN_REASON_NO_MATCHING_PAYMENT_METHOD = 0,
    NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD = 1,
    NOT_SHOWN_REASON_CONCURRENT_REQUESTS = 2,
    NOT_SHOWN_REASON_OTHER = 3,
    NOT_SHOWN_REASON_MAX = 4,
  };

  JourneyLogger(bool is_incognito,
                const GURL& url,
                ukm::UkmRecorder* ukm_recorder);
  ~JourneyLogger();

  // Increments the number of selection adds for the specified section.
  void IncrementSelectionAdds(Section section);

  // Increments the number of selection changes for the specified section.
  void IncrementSelectionChanges(Section section);

  // Increments the number of selection edits for the specified section.
  void IncrementSelectionEdits(Section section);

  // Sets the number of suggestions shown for the specified section.
  void SetNumberOfSuggestionsShown(Section section,
                                   int number,
                                   bool has_valid_suggestion);

  // Records the fact that the merchant called CanMakePayment and records it's
  // return value.
  void SetCanMakePaymentValue(bool value);

  // Records the fact that the Payment Request was shown to the user.
  void SetShowCalled();

  // Records that an event occurred.
  void SetEventOccurred(Event event);

  // Records the payment method that was used to complete the Payment Request.
  void SetSelectedPaymentMethod(SelectedPaymentMethod payment_method);

  // Records the user information requested by the merchant.
  void SetRequestedInformation(bool requested_shipping,
                               bool requested_email,
                               bool requested_phone,
                               bool requested_name);

  // Records that the Payment Request was completed successfully, and starts the
  // logging of all the journey metrics.
  void SetCompleted();

  // Records that the Payment Request was aborted along with the reason. Also
  // starts the logging of all the journey metrics.
  void SetAborted(AbortReason reason);

  // Records that the Payment Request was not shown to the user, along with the
  // reason.
  void SetNotShown(NotShownReason reason);

 private:
  static const int NUMBER_OF_SECTIONS = 3;

  // Note: These constants should always be in sync with their counterpart in
  // components/payments/content/android/java/src/org/chromium/components/
  // payments/JourneyLogger.java.
  // The minimum expected value of CustomCountHistograms is always set to 1. It
  // is still possible to log the value 0 to that type of histogram.
  const int MIN_EXPECTED_SAMPLE = 1;
  const int MAX_EXPECTED_SAMPLE = 49;
  const int NUMBER_BUCKETS = 50;

  struct SectionStats {
    SectionStats()
        : number_selection_adds_(0),
          number_selection_changes_(0),
          number_selection_edits_(0),
          number_suggestions_shown_(0),
          is_requested_(false),
          has_complete_suggestion_(false) {}

    int number_selection_adds_;
    int number_selection_changes_;
    int number_selection_edits_;
    int number_suggestions_shown_;
    bool is_requested_;
    bool has_complete_suggestion_;
  };

  // Records the histograms for all the sections that were requested by the
  // merchant and for the usage of the CanMakePayment method and its effect on
  // the transaction. This method should be called when the Payment Request has
  // either been completed or aborted.
  void RecordJourneyStatsHistograms(CompletionStatus completion_status);

  // Records the histograms for all the steps of a complete checkout flow that
  // were reached.
  void RecordCheckoutFlowMetrics();

  // Records the metric about the selected payment method.
  void RecordPaymentMethodMetric();

  // Records the user information that the merchant requested.
  void RecordRequestedInformationMetrics();

  // Records the histograms for all the sections that were requested by the
  // merchant.
  void RecordSectionSpecificStats(CompletionStatus completion_status);

  // Records the metrics related the the CanMakePayment method unless in
  // incognito mode.
  void RecordCanMakePaymentStats(CompletionStatus completion_status);

  // Records CanMakePayment's return value effect on whether the Payment Request
  // was shown or not.
  void RecordCanMakePaymentEffectOnShow();

  // Records the completion status depending on the the usage and return value
  // of the CanMakePaymentMethod.
  void RecordCanMakePaymentEffectOnCompletion(
      CompletionStatus completion_status);

  // Records the Payment Request Url Keyed Metrics.
  void RecordUrlKeyedMetrics(CompletionStatus completion_status);

  SectionStats sections_[NUMBER_OF_SECTIONS];
  bool has_recorded_ = false;
  bool was_can_make_payments_used_ = false;
  bool could_make_payment_ = false;
  bool was_show_called_ = false;
  bool is_incognito_;

  // Accumulates the many events that have happened during the Payment Request.
  int events_;

  // To keep track of the selected payment method.
  SelectedPaymentMethod payment_method_ = SELECTED_PAYMENT_METHOD_MAX;

  // Keeps track of the user information requested by the merchant.
  int requested_information_ = REQUESTED_INFORMATION_MAX;

  const GURL url_;

  // Not owned, will outlive this object.
  ukm::UkmRecorder* ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(JourneyLogger);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_
