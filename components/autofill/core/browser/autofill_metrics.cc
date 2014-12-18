// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

namespace {

enum FieldTypeGroupForMetrics {
  GROUP_AMBIGUOUS = 0,
  GROUP_NAME,
  GROUP_COMPANY,
  GROUP_ADDRESS_LINE_1,
  GROUP_ADDRESS_LINE_2,
  GROUP_ADDRESS_CITY,
  GROUP_ADDRESS_STATE,
  GROUP_ADDRESS_ZIP,
  GROUP_ADDRESS_COUNTRY,
  GROUP_PHONE,
  GROUP_FAX,  // Deprecated.
  GROUP_EMAIL,
  GROUP_CREDIT_CARD_NAME,
  GROUP_CREDIT_CARD_NUMBER,
  GROUP_CREDIT_CARD_DATE,
  GROUP_CREDIT_CARD_TYPE,
  GROUP_PASSWORD,
  GROUP_ADDRESS_LINE_3,
  NUM_FIELD_TYPE_GROUPS_FOR_METRICS
};

}  // namespace

// First, translates |field_type| to the corresponding logical |group| from
// |FieldTypeGroupForMetrics|.  Then, interpolates this with the given |metric|,
// which should be in the range [0, |num_possible_metrics|).
// Returns the interpolated index.
//
// The interpolation maps the pair (|group|, |metric|) to a single index, so
// that all the indicies for a given group are adjacent.  In particular, with
// the groups {AMBIGUOUS, NAME, ...} combining with the metrics {UNKNOWN, MATCH,
// MISMATCH}, we create this set of mapped indices:
// {
//   AMBIGUOUS+UNKNOWN,
//   AMBIGUOUS+MATCH,
//   AMBIGUOUS+MISMATCH,
//   NAME+UNKNOWN,
//   NAME+MATCH,
//   NAME+MISMATCH,
//   ...
// }.
//
// Clients must ensure that |field_type| is one of the types Chrome supports
// natively, e.g. |field_type| must not be a billng address.
// NOTE: This is defined outside of the anonymous namespace so that it can be
// accessed from the unit test file. It is not exposed in the header file,
// however, because it is not intended for consumption outside of the metrics
// implementation.
int GetFieldTypeGroupMetric(ServerFieldType field_type,
                            AutofillMetrics::FieldTypeQualityMetric metric) {
  DCHECK_LT(metric, AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);

  FieldTypeGroupForMetrics group = GROUP_AMBIGUOUS;
  switch (AutofillType(field_type).group()) {
    case NO_GROUP:
      group = GROUP_AMBIGUOUS;
      break;

    case NAME:
    case NAME_BILLING:
      group = GROUP_NAME;
      break;

    case COMPANY:
      group = GROUP_COMPANY;
      break;

    case ADDRESS_HOME:
    case ADDRESS_BILLING:
      switch (AutofillType(field_type).GetStorableType()) {
        case ADDRESS_HOME_LINE1:
          group = GROUP_ADDRESS_LINE_1;
          break;
        case ADDRESS_HOME_LINE2:
          group = GROUP_ADDRESS_LINE_2;
          break;
        case ADDRESS_HOME_LINE3:
          group = GROUP_ADDRESS_LINE_3;
          break;
        case ADDRESS_HOME_CITY:
          group = GROUP_ADDRESS_CITY;
          break;
        case ADDRESS_HOME_STATE:
          group = GROUP_ADDRESS_STATE;
          break;
        case ADDRESS_HOME_ZIP:
          group = GROUP_ADDRESS_ZIP;
          break;
        case ADDRESS_HOME_COUNTRY:
          group = GROUP_ADDRESS_COUNTRY;
          break;
        default:
          NOTREACHED();
          group = GROUP_AMBIGUOUS;
          break;
      }
      break;

    case EMAIL:
      group = GROUP_EMAIL;
      break;

    case PHONE_HOME:
    case PHONE_BILLING:
      group = GROUP_PHONE;
      break;

    case CREDIT_CARD:
      switch (field_type) {
        case CREDIT_CARD_NAME:
          group = GROUP_CREDIT_CARD_NAME;
          break;
        case CREDIT_CARD_NUMBER:
          group = GROUP_CREDIT_CARD_NUMBER;
          break;
        case CREDIT_CARD_TYPE:
          group = GROUP_CREDIT_CARD_TYPE;
          break;
        case CREDIT_CARD_EXP_MONTH:
        case CREDIT_CARD_EXP_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
          group = GROUP_CREDIT_CARD_DATE;
          break;
        default:
          NOTREACHED();
          group = GROUP_AMBIGUOUS;
          break;
      }
      break;

    case PASSWORD_FIELD:
      group = GROUP_PASSWORD;
      break;

    case TRANSACTION:
      NOTREACHED();
      break;
  }

  // Interpolate the |metric| with the |group|, so that all metrics for a given
  // |group| are adjacent.
  return (group * AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS) + metric;
}

namespace {

std::string WalletApiMetricToString(
    AutofillMetrics::WalletApiCallMetric metric) {
  switch (metric) {
    case AutofillMetrics::ACCEPT_LEGAL_DOCUMENTS:
      return "AcceptLegalDocuments";
    case AutofillMetrics::AUTHENTICATE_INSTRUMENT:
      return "AuthenticateInstrument";
    case AutofillMetrics::GET_FULL_WALLET:
      return "GetFullWallet";
    case AutofillMetrics::GET_WALLET_ITEMS:
      return "GetWalletItems";
    case AutofillMetrics::SAVE_TO_WALLET:
      return "SaveToWallet";
    case AutofillMetrics::UNKNOWN_API_CALL:
    case AutofillMetrics::NUM_WALLET_API_CALLS:
      NOTREACHED();
      return "UnknownApiCall";
  }

  NOTREACHED();
  return "UnknownApiCall";
}

// A version of the UMA_HISTOGRAM_ENUMERATION macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramEnumeration(const std::string& name,
                                int sample,
                                int boundary_value) {
  DCHECK_LT(sample, boundary_value);

  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

// A version of the UMA_HISTOGRAM_TIMES macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramTimes(const std::string& name,
                          const base::TimeDelta& duration) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::Histogram::FactoryTimeGet(
          name,
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromSeconds(10),
          50,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(duration);
}

// A version of the UMA_HISTOGRAM_LONG_TIMES macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramLongTimes(const std::string& name,
                              const base::TimeDelta& duration) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::Histogram::FactoryTimeGet(
          name,
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromHours(1),
          50,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(duration);
}

// Logs a type quality metric.  The primary histogram name is constructed based
// on |base_name|.  The field-specific histogram name also factors in the
// |field_type|.  Logs a sample of |metric|, which should be in the range
// [0, |num_possible_metrics|).
void LogTypeQualityMetric(const std::string& base_name,
                          AutofillMetrics::FieldTypeQualityMetric metric,
                          ServerFieldType field_type) {
  DCHECK_LT(metric, AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);

  LogUMAHistogramEnumeration(base_name, metric,
                             AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);

  int field_type_group_metric = GetFieldTypeGroupMetric(field_type, metric);
  int num_field_type_group_metrics =
      AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS *
      NUM_FIELD_TYPE_GROUPS_FOR_METRICS;
  LogUMAHistogramEnumeration(base_name + ".ByFieldType",
                             field_type_group_metric,
                             num_field_type_group_metrics);
}

}  // namespace

// static
void AutofillMetrics::LogCreditCardInfoBarMetric(InfoBarMetric metric) {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardInfoBar", metric,
                            NUM_INFO_BAR_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardPromptMetric(
    ScanCreditCardPromptMetric metric) {
  DCHECK_LT(metric, NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ScanCreditCardPrompt", metric,
                            NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogDialogDismissalState(DialogDismissalState state) {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.DismissalState",
                            state, NUM_DIALOG_DISMISSAL_STATES);
}

// static
void AutofillMetrics::LogDialogInitialUserState(
    DialogInitialUserStateMetric user_type) {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.InitialUserState",
                            user_type, NUM_DIALOG_INITIAL_USER_STATE_METRICS);
}

// static
void AutofillMetrics::LogDialogLatencyToShow(const base::TimeDelta& duration) {
  LogUMAHistogramTimes("RequestAutocomplete.UiLatencyToShow", duration);
}

// static
void AutofillMetrics::LogDialogPopupEvent(DialogPopupEvent event) {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.PopupInDialog",
                            event, NUM_DIALOG_POPUP_EVENTS);
}

// static
void AutofillMetrics::LogDialogSecurityMetric(DialogSecurityMetric metric) {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.Security",
                            metric, NUM_DIALOG_SECURITY_METRICS);
}

// static
void AutofillMetrics::LogDialogUiDuration(
    const base::TimeDelta& duration,
    DialogDismissalAction dismissal_action) {
  std::string suffix;
  switch (dismissal_action) {
    case DIALOG_ACCEPTED:
      suffix = "Submit";
      break;

    case DIALOG_CANCELED:
      suffix = "Cancel";
      break;
  }

  LogUMAHistogramLongTimes("RequestAutocomplete.UiDuration", duration);
  LogUMAHistogramLongTimes("RequestAutocomplete.UiDuration." + suffix,
                           duration);
}

// static
void AutofillMetrics::LogDialogUiEvent(DialogUiEvent event) {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.UiEvents", event,
                            NUM_DIALOG_UI_EVENTS);
}

// static
void AutofillMetrics::LogWalletErrorMetric(WalletErrorMetric metric) {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.WalletErrors", metric,
                            NUM_WALLET_ERROR_METRICS);
}

// static
void AutofillMetrics::LogWalletApiCallDuration(
    WalletApiCallMetric metric,
    const base::TimeDelta& duration) {
  LogUMAHistogramTimes("Wallet.ApiCallDuration." +
                       WalletApiMetricToString(metric), duration);
}

// static
void AutofillMetrics::LogWalletMalformedResponseMetric(
    WalletApiCallMetric metric) {
  UMA_HISTOGRAM_ENUMERATION("Wallet.MalformedResponse", metric,
                            NUM_WALLET_API_CALLS);
}

// static
void AutofillMetrics::LogWalletRequiredActionMetric(
    WalletRequiredActionMetric required_action) {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.WalletRequiredActions",
                            required_action, NUM_WALLET_REQUIRED_ACTIONS);
}

// static
void AutofillMetrics::LogWalletResponseCode(int response_code) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Wallet.ResponseCode", response_code);
}

// static
void AutofillMetrics::LogDeveloperEngagementMetric(
    DeveloperEngagementMetric metric) {
  DCHECK_LT(metric, NUM_DEVELOPER_ENGAGEMENT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.DeveloperEngagement", metric,
                            NUM_DEVELOPER_ENGAGEMENT_METRICS);
}

// static
void AutofillMetrics::LogHeuristicTypePrediction(FieldTypeQualityMetric metric,
                                                 ServerFieldType field_type) {
  LogTypeQualityMetric("Autofill.Quality.HeuristicType", metric, field_type);
}

// static
void AutofillMetrics::LogOverallTypePrediction(FieldTypeQualityMetric metric,
                                               ServerFieldType field_type) {
  LogTypeQualityMetric("Autofill.Quality.PredictedType", metric, field_type);
}

// static
void AutofillMetrics::LogServerTypePrediction(FieldTypeQualityMetric metric,
                                              ServerFieldType field_type) {
  LogTypeQualityMetric("Autofill.Quality.ServerType", metric, field_type);
}

// static
void AutofillMetrics::LogServerQueryMetric(ServerQueryMetric metric) {
  DCHECK_LT(metric, NUM_SERVER_QUERY_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

// static
void AutofillMetrics::LogUserHappinessMetric(UserHappinessMetric metric) {
  DCHECK_LT(metric, NUM_USER_HAPPINESS_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness", metric,
                            NUM_USER_HAPPINESS_METRICS);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Autofill.FillDuration.FromLoad.WithAutofill",
                             duration,
                             base::TimeDelta::FromMilliseconds(100),
                             base::TimeDelta::FromMinutes(10),
                             50);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Autofill.FillDuration.FromLoad.WithoutAutofill",
                             duration,
                             base::TimeDelta::FromMilliseconds(100),
                             base::TimeDelta::FromMinutes(10),
                             50);
}

// static
void AutofillMetrics::LogFormFillDurationFromInteractionWithAutofill(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Autofill.FillDuration.FromInteraction.WithAutofill",
      duration,
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMinutes(10),
      50);
}

// static
void AutofillMetrics::LogFormFillDurationFromInteractionWithoutAutofill(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
       "Autofill.FillDuration.FromInteraction.WithoutAutofill",
       duration,
       base::TimeDelta::FromMilliseconds(100),
       base::TimeDelta::FromMinutes(10),
       50);
}

// static
void AutofillMetrics::LogIsAutofillEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.Startup", enabled);
}

// static
void AutofillMetrics::LogIsAutofillEnabledAtPageLoad(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.PageLoad", enabled);
}

// static
void AutofillMetrics::LogStoredProfileCount(size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS("Autofill.StoredProfileCount", num_profiles);
}

// static
void AutofillMetrics::LogAddressSuggestionsCount(size_t num_suggestions) {
  UMA_HISTOGRAM_COUNTS("Autofill.AddressSuggestionsCount", num_suggestions);
}

}  // namespace autofill
