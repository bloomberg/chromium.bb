// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/autofill_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/form_structure.h"
#include "components/autofill/common/form_data.h"

namespace {

// Server experiments we support.
enum ServerExperiment {
  NO_EXPERIMENT = 0,
  UNKNOWN_EXPERIMENT,
  ACCEPTANCE_RATIO_06,
  ACCEPTANCE_RATIO_1,
  ACCEPTANCE_RATIO_2,
  ACCEPTANCE_RATIO_4,
  ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_15,
  ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_25,
  ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_15_MIN_FORM_SCORE_5,
  TOOLBAR_DATA_ONLY,
  ACCEPTANCE_RATIO_04_WINNER_LEAD_RATIO_3_MIN_FORM_SCORE_4,
  NO_SERVER_RESPONSE,
  PROBABILITY_PICKER_05,
  PROBABILITY_PICKER_025,
  PROBABILITY_PICKER_025_CC_THRESHOLD_03,
  PROBABILITY_PICKER_025_CONTEXTUAL_CC_THRESHOLD_03,
  PROBABILITY_PICKER_025_CONTEXTUAL_CC_THRESHOLD_03_WITH_FALLBACK,
  PROBABILITY_PICKER_05_CC_NAME_THRESHOLD_03_EXPERIMENT_1,
  NUM_SERVER_EXPERIMENTS
};

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
int GetFieldTypeGroupMetric(const AutofillFieldType field_type,
                            const int metric,
                            const int num_possible_metrics) {
  DCHECK_LT(metric, num_possible_metrics);

  FieldTypeGroupForMetrics group;
  switch (AutofillType(field_type).group()) {
    case AutofillType::NO_GROUP:
      group = AMBIGUOUS;
      break;

    case AutofillType::NAME:
      group = NAME;
      break;

    case AutofillType::COMPANY:
      group = COMPANY;
      break;

    case AutofillType::ADDRESS_HOME:
      switch (field_type) {
        case ADDRESS_HOME_LINE1:
          group = ADDRESS_LINE_1;
          break;
        case ADDRESS_HOME_LINE2:
          group = ADDRESS_LINE_2;
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
      }
      break;

    case AutofillType::EMAIL:
      group = EMAIL;
      break;

    case AutofillType::PHONE:
      group = PHONE;
      break;

    case AutofillType::CREDIT_CARD:
      switch (field_type) {
        case ::CREDIT_CARD_NAME:
          group = CREDIT_CARD_NAME;
          break;
        case ::CREDIT_CARD_NUMBER:
          group = CREDIT_CARD_NUMBER;
          break;
        case ::CREDIT_CARD_TYPE:
          group = CREDIT_CARD_TYPE;
        default:
          group = CREDIT_CARD_DATE;
      }
      break;

    default:
      NOTREACHED();
      group = AMBIGUOUS;
  }

  // Interpolate the |metric| with the |group|, so that all metrics for a given
  // |group| are adjacent.
  return (group * num_possible_metrics) + metric;
}

// Returns the histogram prefix to use for reporting metrics for |dialog_type|.
std::string GetPrefixForDialogType(autofill::DialogType dialog_type) {
  switch (dialog_type) {
    case autofill::DIALOG_TYPE_AUTOCHECKOUT:
      return "Autocheckout";

    case autofill::DIALOG_TYPE_REQUEST_AUTOCOMPLETE:
      return "RequestAutocomplete";
  }

  NOTREACHED();
  return "UnknownDialogType";
}

std::string WalletApiMetricToString(
    AutofillMetrics::WalletApiCallMetric metric) {
  switch (metric) {
    case AutofillMetrics::ACCEPT_LEGAL_DOCUMENTS:
      return "ACCEPT_LEGAL_DOCUMENTS";
    case AutofillMetrics::AUTHENTICATE_INSTRUMENT:
      return "AUTHENTICATE_INSTRUMENT";
    case AutofillMetrics::GET_FULL_WALLET:
      return "GET_FULL_WALLET";
    case AutofillMetrics::GET_WALLET_ITEMS:
      return "GET_WALLET_ITEMS";
    case AutofillMetrics::SAVE_ADDRESS:
      return "SAVE_ADDRESS";
    case AutofillMetrics::SAVE_INSTRUMENT:
      return "SAVE_INSTRUMENT";
    case AutofillMetrics::SAVE_INSTRUMENT_AND_ADDRESS:
      return "SAVE_INSTRUMENT_AND_ADDRESS";
    case AutofillMetrics::SEND_STATUS:
      return "SEND_STATUS";
    case AutofillMetrics::UPDATE_ADDRESS:
      return "UPDATE_ADDRESS";
    case AutofillMetrics::UPDATE_INSTRUMENT:
      return "UPDATE_INSTRUMENT";
    case AutofillMetrics::UNKNOWN_API_CALL:
      NOTREACHED();
      return "UNKNOWN_API_CALL";
  }

  NOTREACHED();
  return "UNKNOWN_API_CALL";
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
// on |base_name| and |experiment_id|.  The field-specific histogram name also
// factors in the |field_type|.  Logs a sample of |metric|, which should be in
// the range [0, |num_possible_metrics|).
void LogTypeQualityMetric(const std::string& base_name,
                          const int metric,
                          const int num_possible_metrics,
                          const AutofillFieldType field_type,
                          const std::string& experiment_id) {
  DCHECK_LT(metric, num_possible_metrics);

  std::string histogram_name = base_name;
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;
  LogUMAHistogramEnumeration(histogram_name, metric, num_possible_metrics);

  std::string sub_histogram_name = base_name + ".ByFieldType";
  if (!experiment_id.empty())
    sub_histogram_name += "_" + experiment_id;
  const int field_type_group_metric =
      GetFieldTypeGroupMetric(field_type, metric, num_possible_metrics);
  const int num_field_type_group_metrics =
      num_possible_metrics * NUM_FIELD_TYPE_GROUPS_FOR_METRICS;
  LogUMAHistogramEnumeration(sub_histogram_name,
                             field_type_group_metric,
                             num_field_type_group_metrics);
}

void LogServerExperimentId(const std::string& histogram_name,
                           const std::string& experiment_id) {
  ServerExperiment metric = UNKNOWN_EXPERIMENT;

  const std::string default_experiment_name =
      FormStructure(FormData(), std::string()).server_experiment_id();
  if (experiment_id.empty())
    metric = NO_EXPERIMENT;
  else if (experiment_id == "ar06")
    metric = ACCEPTANCE_RATIO_06;
  else if (experiment_id == "ar1")
    metric = ACCEPTANCE_RATIO_1;
  else if (experiment_id == "ar2")
    metric = ACCEPTANCE_RATIO_2;
  else if (experiment_id == "ar4")
    metric = ACCEPTANCE_RATIO_4;
  else if (experiment_id == "ar05wlr15")
    metric = ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_15;
  else if (experiment_id == "ar05wlr25")
    metric = ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_25;
  else if (experiment_id == "ar05wr15fs5")
    metric = ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_15_MIN_FORM_SCORE_5;
  else if (experiment_id == "tbar1")
    metric = TOOLBAR_DATA_ONLY;
  else if (experiment_id == "ar04wr3fs4")
    metric = ACCEPTANCE_RATIO_04_WINNER_LEAD_RATIO_3_MIN_FORM_SCORE_4;
  else if (experiment_id == default_experiment_name)
    metric = NO_SERVER_RESPONSE;
  else if (experiment_id == "fp05")
    metric = PROBABILITY_PICKER_05;
  else if (experiment_id == "fp025")
    metric = PROBABILITY_PICKER_025;
  else if (experiment_id == "fp05cc03")
    metric = PROBABILITY_PICKER_025_CC_THRESHOLD_03;
  else if (experiment_id == "fp05cco03")
    metric = PROBABILITY_PICKER_025_CONTEXTUAL_CC_THRESHOLD_03;
  else if (experiment_id == "fp05cco03cstd")
    metric = PROBABILITY_PICKER_025_CONTEXTUAL_CC_THRESHOLD_03_WITH_FALLBACK;
  else if (experiment_id == "fp05cc03e1")
    metric = PROBABILITY_PICKER_05_CC_NAME_THRESHOLD_03_EXPERIMENT_1;

  DCHECK_LT(metric, NUM_SERVER_EXPERIMENTS);
  LogUMAHistogramEnumeration(histogram_name, metric, NUM_SERVER_EXPERIMENTS);
}

}  // namespace

AutofillMetrics::AutofillMetrics() {
}

AutofillMetrics::~AutofillMetrics() {
}

void AutofillMetrics::LogAutocheckoutBubbleMetric(BubbleMetric metric) const {
  DCHECK_LT(metric, NUM_BUBBLE_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autocheckout.Bubble", metric, NUM_BUBBLE_METRICS);
}

void AutofillMetrics::LogAutocheckoutBuyFlowMetric(
    AutocheckoutBuyFlowMetric metric) const {
  DCHECK_LT(metric, NUM_AUTOCHECKOUT_BUY_FLOW_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autocheckout.BuyFlow", metric,
                            NUM_AUTOCHECKOUT_BUY_FLOW_METRICS);
}

void AutofillMetrics::LogCreditCardInfoBarMetric(InfoBarMetric metric) const {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardInfoBar", metric,
                            NUM_INFO_BAR_METRICS);
}

void AutofillMetrics::LogDialogInitialUserState(
    autofill::DialogType dialog_type,
    DialogInitialUserStateMetric user_type) const {
  std::string name = GetPrefixForDialogType(dialog_type) + ".InitialUserState";
  LogUMAHistogramEnumeration(
      name, user_type, NUM_DIALOG_INITIAL_USER_STATE_METRICS);
}

void AutofillMetrics::LogDialogPopupEvent(autofill::DialogType dialog_type,
                                          DialogPopupEvent event) const {
  std::string name = GetPrefixForDialogType(dialog_type) + ".PopupInDialog";
  LogUMAHistogramEnumeration(name, event, NUM_DIALOG_POPUP_EVENTS);
}

void AutofillMetrics::LogDialogSecurityMetric(
    autofill::DialogType dialog_type,
    DialogSecurityMetric metric) const {
  std::string name = GetPrefixForDialogType(dialog_type) + ".Security";
  LogUMAHistogramEnumeration(name, metric, NUM_DIALOG_SECURITY_METRICS);
}

void AutofillMetrics::LogDialogUiDuration(
    const base::TimeDelta& duration,
    autofill::DialogType dialog_type,
    DialogDismissalAction dismissal_action) const {
  std::string prefix = GetPrefixForDialogType(dialog_type);

  std::string suffix;
  switch (dismissal_action) {
    case DIALOG_ACCEPTED:
      suffix = "Submit";
      break;

    case DIALOG_CANCELED:
      suffix = "Cancel";
      break;
  }

  LogUMAHistogramLongTimes(prefix + ".UiDuration", duration);
  LogUMAHistogramLongTimes(prefix + ".UiDuration." + suffix, duration);
}

void AutofillMetrics::LogWalletErrorMetric(autofill::DialogType dialog_type,
                                           WalletErrorMetric metric) const {
  std::string name = GetPrefixForDialogType(dialog_type) + ".WalletErrors";
  LogUMAHistogramEnumeration(name, metric, NUM_WALLET_ERROR_METRICS);
}

void AutofillMetrics::LogWalletApiCallDuration(
    WalletApiCallMetric metric,
    const base::TimeDelta& duration) const {
  LogUMAHistogramTimes("Wallet." + WalletApiMetricToString(metric) +
                       ".ApiCallDuration", duration);
}

void AutofillMetrics::LogWalletRequiredActionMetric(
      autofill::DialogType dialog_type,
      WalletRequiredActionMetric required_action) const {
  std::string name =
      GetPrefixForDialogType(dialog_type) + ".WalletRequiredActions";
  LogUMAHistogramEnumeration(
      name, required_action, NUM_WALLET_REQUIRED_ACTIONS);
}

void AutofillMetrics::LogAutocheckoutDuration(
    const base::TimeDelta& duration,
    AutocheckoutCompletionStatus status) const {
  std::string suffix;
  switch (status) {
    case AUTOCHECKOUT_FAILED:
      suffix = "Failed";
      break;

    case AUTOCHECKOUT_SUCCEEDED:
      suffix = "Succeeded";
      break;
  }

  LogUMAHistogramLongTimes("Autocheckout.FlowDuration", duration);
  LogUMAHistogramLongTimes("Autocheckout.FlowDuration." + suffix, duration);
}

void AutofillMetrics::LogDeveloperEngagementMetric(
    DeveloperEngagementMetric metric) const {
  DCHECK_LT(metric, NUM_DEVELOPER_ENGAGEMENT_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.DeveloperEngagement", metric,
                            NUM_DEVELOPER_ENGAGEMENT_METRICS);
}

void AutofillMetrics::LogHeuristicTypePrediction(
    FieldTypeQualityMetric metric,
    AutofillFieldType field_type,
    const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.HeuristicType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
}

void AutofillMetrics::LogOverallTypePrediction(
    FieldTypeQualityMetric metric,
    AutofillFieldType field_type,
    const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.PredictedType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
}

void AutofillMetrics::LogServerTypePrediction(
    FieldTypeQualityMetric metric,
    AutofillFieldType field_type,
    const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.ServerType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
}

void AutofillMetrics::LogQualityMetric(QualityMetric metric,
                                       const std::string& experiment_id) const {
  DCHECK_LT(metric, NUM_QUALITY_METRICS);

  std::string histogram_name = "Autofill.Quality";
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;

  LogUMAHistogramEnumeration(histogram_name, metric, NUM_QUALITY_METRICS);
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

void AutofillMetrics::LogServerExperimentIdForQuery(
    const std::string& experiment_id) const {
  LogServerExperimentId("Autofill.ServerExperimentId.Query", experiment_id);
}

void AutofillMetrics::LogServerExperimentIdForUpload(
    const std::string& experiment_id) const {
  LogServerExperimentId("Autofill.ServerExperimentId.Upload", experiment_id);
}
