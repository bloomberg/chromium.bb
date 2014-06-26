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
  AMBIGUOUS = 0,
  NAME,
  COMPANY,
  ADDRESS_LINE_1,
  ADDRESS_LINE_2,
  ADDRESS_CITY,
  ADDRESS_STATE,
  ADDRESS_ZIP,
  ADDRESS_COUNTRY,
  PHONE,
  FAX,  // Deprecated.
  EMAIL,
  CREDIT_CARD_NAME,
  CREDIT_CARD_NUMBER,
  CREDIT_CARD_DATE,
  CREDIT_CARD_TYPE,
  PASSWORD,
  ADDRESS_LINE_3,
  NUM_FIELD_TYPE_GROUPS_FOR_METRICS
};

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
int GetFieldTypeGroupMetric(const ServerFieldType field_type,
                            const int metric,
                            const int num_possible_metrics) {
  DCHECK_LT(metric, num_possible_metrics);

  FieldTypeGroupForMetrics group = AMBIGUOUS;
  switch (AutofillType(field_type).group()) {
    case ::autofill::NO_GROUP:
      group = AMBIGUOUS;
      break;

    case ::autofill::NAME:
    case ::autofill::NAME_BILLING:
      group = NAME;
      break;

    case ::autofill::COMPANY:
      group = COMPANY;
      break;

    case ::autofill::ADDRESS_HOME:
    case ::autofill::ADDRESS_BILLING:
      switch (AutofillType(field_type).GetStorableType()) {
        case ADDRESS_HOME_LINE1:
          group = ADDRESS_LINE_1;
          break;
        case ADDRESS_HOME_LINE2:
          group = ADDRESS_LINE_2;
          break;
        case ADDRESS_HOME_LINE3:
          group = ADDRESS_LINE_3;
          break;
        case ADDRESS_HOME_CITY:
          group = ADDRESS_CITY;
          break;
        case ADDRESS_HOME_STATE:
          group = ADDRESS_STATE;
          break;
        case ADDRESS_HOME_ZIP:
          group = ADDRESS_ZIP;
          break;
        case ADDRESS_HOME_COUNTRY:
          group = ADDRESS_COUNTRY;
          break;
        default:
          NOTREACHED();
          group = AMBIGUOUS;
          break;
      }
      break;

    case ::autofill::EMAIL:
      group = EMAIL;
      break;

    case ::autofill::PHONE_HOME:
    case ::autofill::PHONE_BILLING:
      group = PHONE;
      break;

    case ::autofill::CREDIT_CARD:
      switch (field_type) {
        case ::autofill::CREDIT_CARD_NAME:
          group = CREDIT_CARD_NAME;
          break;
        case ::autofill::CREDIT_CARD_NUMBER:
          group = CREDIT_CARD_NUMBER;
          break;
        case ::autofill::CREDIT_CARD_TYPE:
          group = CREDIT_CARD_TYPE;
          break;
        case ::autofill::CREDIT_CARD_EXP_MONTH:
        case ::autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR:
        case ::autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
        case ::autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
        case ::autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
          group = CREDIT_CARD_DATE;
          break;
        default:
          NOTREACHED();
          group = AMBIGUOUS;
          break;
      }
      break;

    case ::autofill::PASSWORD_FIELD:
      group = PASSWORD;
      break;

    case ::autofill::TRANSACTION:
      NOTREACHED();
      break;
  }

  // Interpolate the |metric| with the |group|, so that all metrics for a given
  // |group| are adjacent.
  return (group * num_possible_metrics) + metric;
}

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
                          const int metric,
                          const int num_possible_metrics,
                          const ServerFieldType field_type) {
  DCHECK_LT(metric, num_possible_metrics);

  std::string histogram_name = base_name;
  LogUMAHistogramEnumeration(histogram_name, metric, num_possible_metrics);

  std::string sub_histogram_name = base_name + ".ByFieldType";
  const int field_type_group_metric =
      GetFieldTypeGroupMetric(field_type, metric, num_possible_metrics);
  const int num_field_type_group_metrics =
      num_possible_metrics * NUM_FIELD_TYPE_GROUPS_FOR_METRICS;
  LogUMAHistogramEnumeration(sub_histogram_name,
                             field_type_group_metric,
                             num_field_type_group_metrics);
}

}  // namespace

AutofillMetrics::AutofillMetrics() {
}

AutofillMetrics::~AutofillMetrics() {
}

void AutofillMetrics::LogCreditCardInfoBarMetric(InfoBarMetric metric) const {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardInfoBar", metric,
                            NUM_INFO_BAR_METRICS);
}

void AutofillMetrics::LogDialogDismissalState(
    DialogDismissalState state) const {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.DismissalState",
                            state, NUM_DIALOG_DISMISSAL_STATES);
}

void AutofillMetrics::LogDialogInitialUserState(
    DialogInitialUserStateMetric user_type) const {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.InitialUserState",
                            user_type, NUM_DIALOG_INITIAL_USER_STATE_METRICS);
}

void AutofillMetrics::LogDialogLatencyToShow(
    const base::TimeDelta& duration) const {
  LogUMAHistogramTimes("RequestAutocomplete.UiLatencyToShow", duration);
}

void AutofillMetrics::LogDialogPopupEvent(DialogPopupEvent event) const {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.PopupInDialog",
                            event, NUM_DIALOG_POPUP_EVENTS);
}

void AutofillMetrics::LogDialogSecurityMetric(
    DialogSecurityMetric metric) const {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.Security",
                            metric, NUM_DIALOG_SECURITY_METRICS);
}

void AutofillMetrics::LogDialogUiDuration(
    const base::TimeDelta& duration,
    DialogDismissalAction dismissal_action) const {
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

void AutofillMetrics::LogDialogUiEvent(DialogUiEvent event) const {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.UiEvents", event,
                            NUM_DIALOG_UI_EVENTS);
}

void AutofillMetrics::LogWalletErrorMetric(WalletErrorMetric metric) const {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.WalletErrors", metric,
                            NUM_WALLET_ERROR_METRICS);
}

void AutofillMetrics::LogWalletApiCallDuration(
    WalletApiCallMetric metric,
    const base::TimeDelta& duration) const {
  LogUMAHistogramTimes("Wallet.ApiCallDuration." +
                       WalletApiMetricToString(metric), duration);
}

void AutofillMetrics::LogWalletMalformedResponseMetric(
    WalletApiCallMetric metric) const {
  UMA_HISTOGRAM_ENUMERATION("Wallet.MalformedResponse", metric,
                            NUM_WALLET_API_CALLS);
}

void AutofillMetrics::LogWalletRequiredActionMetric(
      WalletRequiredActionMetric required_action) const {
  UMA_HISTOGRAM_ENUMERATION("RequestAutocomplete.WalletRequiredActions",
                            required_action, NUM_WALLET_REQUIRED_ACTIONS);
}

void AutofillMetrics::LogWalletResponseCode(int response_code) const {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Wallet.ResponseCode", response_code);
}

void AutofillMetrics::LogDeveloperEngagementMetric(
    DeveloperEngagementMetric metric) const {
  DCHECK_LT(metric, NUM_DEVELOPER_ENGAGEMENT_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.DeveloperEngagement", metric,
                            NUM_DEVELOPER_ENGAGEMENT_METRICS);
}

void AutofillMetrics::LogHeuristicTypePrediction(
    FieldTypeQualityMetric metric,
    ServerFieldType field_type) const {
  LogTypeQualityMetric("Autofill.Quality.HeuristicType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS, field_type);
}

void AutofillMetrics::LogOverallTypePrediction(
    FieldTypeQualityMetric metric,
    ServerFieldType field_type) const {
  LogTypeQualityMetric("Autofill.Quality.PredictedType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS, field_type);
}

void AutofillMetrics::LogServerTypePrediction(
    FieldTypeQualityMetric metric,
    ServerFieldType field_type) const {
  LogTypeQualityMetric("Autofill.Quality.ServerType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS, field_type);
}

void AutofillMetrics::LogServerQueryMetric(ServerQueryMetric metric) const {
  DCHECK_LT(metric, NUM_SERVER_QUERY_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

void AutofillMetrics::LogUserHappinessMetric(UserHappinessMetric metric) const {
  DCHECK_LT(metric, NUM_USER_HAPPINESS_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness", metric,
                            NUM_USER_HAPPINESS_METRICS);
}

void AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(
    const base::TimeDelta& duration) const {
  UMA_HISTOGRAM_CUSTOM_TIMES("Autofill.FillDuration.FromLoad.WithAutofill",
                             duration,
                             base::TimeDelta::FromMilliseconds(100),
                             base::TimeDelta::FromMinutes(10),
                             50);
}

void AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(
    const base::TimeDelta& duration) const {
  UMA_HISTOGRAM_CUSTOM_TIMES("Autofill.FillDuration.FromLoad.WithoutAutofill",
                             duration,
                             base::TimeDelta::FromMilliseconds(100),
                             base::TimeDelta::FromMinutes(10),
                             50);
}

void AutofillMetrics::LogFormFillDurationFromInteractionWithAutofill(
    const base::TimeDelta& duration) const {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Autofill.FillDuration.FromInteraction.WithAutofill",
      duration,
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMinutes(10),
      50);
}

void AutofillMetrics::LogFormFillDurationFromInteractionWithoutAutofill(
    const base::TimeDelta& duration) const {
  UMA_HISTOGRAM_CUSTOM_TIMES(
       "Autofill.FillDuration.FromInteraction.WithoutAutofill",
       duration,
       base::TimeDelta::FromMilliseconds(100),
       base::TimeDelta::FromMinutes(10),
       50);
}

void AutofillMetrics::LogIsAutofillEnabledAtStartup(bool enabled) const {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.Startup", enabled);
}

void AutofillMetrics::LogIsAutofillEnabledAtPageLoad(bool enabled) const {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.PageLoad", enabled);
}

void AutofillMetrics::LogStoredProfileCount(size_t num_profiles) const {
  UMA_HISTOGRAM_COUNTS("Autofill.StoredProfileCount", num_profiles);
}

void AutofillMetrics::LogAddressSuggestionsCount(size_t num_suggestions) const {
  UMA_HISTOGRAM_COUNTS("Autofill.AddressSuggestionsCount", num_suggestions);
}

}  // namespace autofill
