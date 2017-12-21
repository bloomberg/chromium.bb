// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_metrics.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill {

namespace {

// Note: if adding an enum value here, update the corresponding description for
// AutofillTypeQualityByFieldType in histograms.xml.
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
  GROUP_USERNAME,
  GROUP_STREET_ADDRESS,
  GROUP_CREDIT_CARD_VERIFICATION,
  NUM_FIELD_TYPE_GROUPS_FOR_METRICS
};

const int KMaxFieldTypeGroupMetric =
    (NUM_FIELD_TYPE_GROUPS_FOR_METRICS << 8) |
    AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS;

std::string PreviousSaveCreditCardPromptUserDecisionToString(
    int previous_save_credit_card_prompt_user_decision) {
  DCHECK_LT(previous_save_credit_card_prompt_user_decision,
            prefs::NUM_PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISIONS);
  std::string previous_response;
  if (previous_save_credit_card_prompt_user_decision ==
      prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED)
    previous_response = ".PreviouslyAccepted";
  else if (previous_save_credit_card_prompt_user_decision ==
           prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED)
    previous_response = ".PreviouslyDenied";
  else
    DCHECK_EQ(previous_save_credit_card_prompt_user_decision,
              prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE);
  return previous_response;
}

ukm::SourceId NewUkmSourceWithUrl(ukm::UkmRecorder* ukm_recorder,
                                  const GURL& url) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  AutofillMetrics::UpdateSourceURL(ukm_recorder, source_id, url);
  return source_id;
}

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
        case ADDRESS_HOME_STREET_ADDRESS:
          group = GROUP_STREET_ADDRESS;
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
          NOTREACHED() << field_type << " has no group assigned (ambiguous)";
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
        case CREDIT_CARD_NAME_FULL:
        case CREDIT_CARD_NAME_FIRST:
        case CREDIT_CARD_NAME_LAST:
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
        case CREDIT_CARD_VERIFICATION_CODE:
          group = GROUP_CREDIT_CARD_VERIFICATION;
          break;
        default:
          NOTREACHED() << field_type << " has no group assigned (ambiguous)";
          group = GROUP_AMBIGUOUS;
          break;
      }
      break;

    case PASSWORD_FIELD:
      group = GROUP_PASSWORD;
      break;

    case USERNAME_FIELD:
      group = GROUP_USERNAME;
      break;

    case TRANSACTION:
      NOTREACHED();
      break;
  }

  // Use bits 8-15 for the group and bits 0-7 for the metric.
  static_assert(AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS <= UINT8_MAX,
                "maximum field type quality metric must fit into 8 bits");
  static_assert(NUM_FIELD_TYPE_GROUPS_FOR_METRICS <= UINT8_MAX,
                "number of field type groups must fit into 8 bits");
  return (group << 8) | metric;
}

namespace {

// A version of the UMA_HISTOGRAM_ENUMERATION macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramEnumeration(const std::string& name,
                                int sample,
                                int boundary_value) {
  DCHECK_LT(sample, boundary_value);

  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      name, 1, boundary_value, boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

// A version of the UMA_HISTOGRAM_LONG_TIMES macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramLongTimes(const std::string& name,
                              const base::TimeDelta& duration) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      name, base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1),
      50, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(duration);
}

const char* GetQualityMetricPredictionSource(
    AutofillMetrics::QualityMetricPredictionSource source) {
  switch (source) {
    default:
    case AutofillMetrics::PREDICTION_SOURCE_UNKNOWN:
      NOTREACHED();
      return "Unknown";

    case AutofillMetrics::PREDICTION_SOURCE_HEURISTIC:
      return "Heuristic";
    case AutofillMetrics::PREDICTION_SOURCE_SERVER:
      return "Server";
    case AutofillMetrics::PREDICTION_SOURCE_OVERALL:
      return "Overall";
  }
}

const char* GetQualityMetricTypeSuffix(
    AutofillMetrics::QualityMetricType metric_type) {
  switch (metric_type) {
    default:
      NOTREACHED();
    // Fall through...

    case AutofillMetrics::TYPE_SUBMISSION:
      return "";
    case AutofillMetrics::TYPE_NO_SUBMISSION:
      return ".NoSubmission";
    case AutofillMetrics::TYPE_AUTOCOMPLETE_BASED:
      return ".BasedOnAutocomplete";
  }
}

// Given a set of |possible_types| for a field, select the best type to use as
// the "actual" field type when calculating metrics. If the |predicted_type| is
// among the |possible_types] then use that as the best type (i.e., the
// prediction is deemed to have been correct).
ServerFieldType GetActualFieldType(const ServerFieldTypeSet& possible_types,
                                   ServerFieldType predicted_type) {
  DCHECK_NE(possible_types.size(), 0u);

  if (possible_types.count(EMPTY_TYPE)) {
    DCHECK_EQ(possible_types.size(), 1u);
    return EMPTY_TYPE;
  }

  if (possible_types.count(UNKNOWN_TYPE)) {
    DCHECK_EQ(possible_types.size(), 1u);
    return UNKNOWN_TYPE;
  }

  if (possible_types.count(predicted_type))
    return predicted_type;

  // Collapse field types that Chrome treats as identical, e.g. home and
  // billing address fields.
  ServerFieldTypeSet collapsed_field_types;
  for (const auto& type : possible_types) {
    DCHECK_NE(type, EMPTY_TYPE);
    DCHECK_NE(type, UNKNOWN_TYPE);

    // A phone number that's only missing its country code is (for metrics
    // purposes) the same as the whole phone number.
    if (type == PHONE_HOME_CITY_AND_NUMBER)
      collapsed_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
    else
      collapsed_field_types.insert(AutofillType(type).GetStorableType());
  }

  // Capture the field's type, if it is unambiguous.
  ServerFieldType actual_type = AMBIGUOUS_TYPE;
  if (collapsed_field_types.size() == 1)
    actual_type = *collapsed_field_types.begin();

  DVLOG(2) << "Inferred Type: " << AutofillType(actual_type).ToString();
  return actual_type;
}

// Check if the value of |field| is same as one of the previously autofilled
// values. This indicates a bad rationalization if |field| has
// only_fill_when_focued set to true.
bool DuplicatedFilling(const FormStructure& form, const AutofillField& field) {
  for (const auto& form_field : form) {
    if (field.value == form_field->value && form_field->is_autofilled)
      return true;
  }
  return false;
}

void LogPredictionQualityMetricsForFieldsOnlyFilledWhenFocused(
    const std::string& aggregate_histogram,
    const std::string& type_specific_histogram,
    const std::string& rationalization_quality_histogram,
    ServerFieldType predicted_type,
    ServerFieldType actual_type,
    bool is_empty,
    bool is_ambiguous,
    bool log_rationalization_metrics,
    const FormStructure& form,
    const AutofillField& field) {
  // If it is filled with values unknown, it is a true negative.
  if (actual_type == UNKNOWN_TYPE) {
    // Only log aggregate true negative; do not log type specific metrics
    // for UNKNOWN/EMPTY.
    DVLOG(2) << "TRUE NEGATIVE";
    LogUMAHistogramEnumeration(
        aggregate_histogram,
        (is_empty ? AutofillMetrics::TRUE_NEGATIVE_EMPTY
                  : (is_ambiguous ? AutofillMetrics::TRUE_NEGATIVE_AMBIGUOUS
                                  : AutofillMetrics::TRUE_NEGATIVE_UNKNOWN)),
        AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);
    if (log_rationalization_metrics) {
      LogUMAHistogramEnumeration(
          rationalization_quality_histogram,
          (is_empty ? AutofillMetrics::RATIONALIZATION_GOOD
                    : AutofillMetrics::RATIONALIZATION_OK),
          AutofillMetrics::NUM_RATIONALIZATION_QUALITY_METRICS);
    }
    return;
  }

  // If it is filled with same type as predicted, it is a true positive. We
  // also log an RATIONALIZATION_BAD by checking if the filled value is filled
  // already in previous fields, this means autofill could have filled it
  // automatically if there has been no rationalization.
  if (predicted_type == actual_type) {
    DVLOG(2) << "TRUE POSITIVE";
    LogUMAHistogramEnumeration(aggregate_histogram,
                               AutofillMetrics::TRUE_POSITIVE,
                               AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);
    LogUMAHistogramEnumeration(
        type_specific_histogram,
        GetFieldTypeGroupMetric(actual_type, AutofillMetrics::TRUE_POSITIVE),
        KMaxFieldTypeGroupMetric);
    if (log_rationalization_metrics) {
      bool duplicated_filling = DuplicatedFilling(form, field);
      LogUMAHistogramEnumeration(
          rationalization_quality_histogram,
          (duplicated_filling ? AutofillMetrics::RATIONALIZATION_BAD
                              : AutofillMetrics::RATIONALIZATION_OK),
          AutofillMetrics::NUM_RATIONALIZATION_QUALITY_METRICS);
    }
    return;
  }

  DVLOG(2) << "MISMATCH";
  // Here the prediction is wrong, but user has to provide some value still.
  // This should be a false negative.
  LogUMAHistogramEnumeration(aggregate_histogram,
                             AutofillMetrics::FALSE_NEGATIVE_MISMATCH,
                             AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);
  // Log FALSE_NEGATIVE_MISMATCH for predicted type if it did predicted
  // something but actual type is different.
  if (predicted_type != UNKNOWN_TYPE)
    LogUMAHistogramEnumeration(
        type_specific_histogram,
        GetFieldTypeGroupMetric(predicted_type,
                                AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        KMaxFieldTypeGroupMetric);
  if (log_rationalization_metrics) {
    // Logging RATIONALIZATION_OK despite of type mismatch here because autofill
    // would have got it wrong with or without rationalization. Rationalization
    // here does not help, neither does it do any harm.
    LogUMAHistogramEnumeration(
        rationalization_quality_histogram, AutofillMetrics::RATIONALIZATION_OK,
        AutofillMetrics::NUM_RATIONALIZATION_QUALITY_METRICS);
  }
  return;
}

void LogPredictionQualityMetricsForCommonFields(
    const std::string& aggregate_histogram,
    const std::string& type_specific_histogram,
    ServerFieldType predicted_type,
    ServerFieldType actual_type,
    bool is_empty,
    bool is_ambiguous) {
  // If the predicted and actual types match then it's either a true positive
  // or a true negative (if they are both unknown). Do not log type specific
  // true negatives (instead log a true positive for the "Ambiguous" type).
  if (predicted_type == actual_type) {
    if (actual_type == UNKNOWN_TYPE) {
      // Only log aggregate true negative; do not log type specific metrics
      // for UNKNOWN/EMPTY.
      DVLOG(2) << "TRUE NEGATIVE";
      LogUMAHistogramEnumeration(
          aggregate_histogram,
          (is_empty ? AutofillMetrics::TRUE_NEGATIVE_EMPTY
                    : (is_ambiguous ? AutofillMetrics::TRUE_NEGATIVE_AMBIGUOUS
                                    : AutofillMetrics::TRUE_NEGATIVE_UNKNOWN)),
          AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);
      return;
    }

    DVLOG(2) << "TRUE POSITIVE";
    // Log both aggregate and type specific true positive if we correctly
    // predict that type with which the field was filled.
    LogUMAHistogramEnumeration(aggregate_histogram,
                               AutofillMetrics::TRUE_POSITIVE,
                               AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);
    LogUMAHistogramEnumeration(
        type_specific_histogram,
        GetFieldTypeGroupMetric(actual_type, AutofillMetrics::TRUE_POSITIVE),
        KMaxFieldTypeGroupMetric);
    return;
  }

  // Note: At this point predicted_type != actual type
  // If actual type is UNKNOWN_TYPE then the prediction is a false positive.
  // Further specialize the type of false positive by whether the field was
  // empty or contained an unknown value.
  if (actual_type == UNKNOWN_TYPE) {
    DVLOG(2) << "FALSE POSITIVE";
    auto metric =
        (is_empty ? AutofillMetrics::FALSE_POSITIVE_EMPTY
                  : (is_ambiguous ? AutofillMetrics::FALSE_POSITIVE_AMBIGUOUS
                                  : AutofillMetrics::FALSE_POSITIVE_UNKNOWN));
    LogUMAHistogramEnumeration(aggregate_histogram, metric,
                               AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);
    LogUMAHistogramEnumeration(type_specific_histogram,
                               GetFieldTypeGroupMetric(predicted_type, metric),
                               KMaxFieldTypeGroupMetric);
    return;
  }

  // Note: At this point predicted_type != actual type, actual_type != UNKNOWN.
  // If predicted type is UNKNOWN_TYPE then the prediction is a false negative
  // unknown.
  if (predicted_type == UNKNOWN_TYPE) {
    DVLOG(2) << "FALSE NEGATIVE";
    LogUMAHistogramEnumeration(aggregate_histogram,
                               AutofillMetrics::FALSE_NEGATIVE_UNKNOWN,
                               AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);
    LogUMAHistogramEnumeration(
        type_specific_histogram,
        GetFieldTypeGroupMetric(actual_type,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        KMaxFieldTypeGroupMetric);
    return;
  }

  DVLOG(2) << "MISMATCH";

  // Note: At this point predicted_type != actual type, actual_type != UNKNOWN,
  //       predicted_type != UNKNOWN.
  // This is a mismatch. From the reference of the actual type, this is a false
  // negative (it was T, but predicted U). From the reference of the prediction,
  // this is a false positive (predicted it was T, but it was U).
  LogUMAHistogramEnumeration(aggregate_histogram,
                             AutofillMetrics::FALSE_NEGATIVE_MISMATCH,
                             AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);
  LogUMAHistogramEnumeration(
      type_specific_histogram,
      GetFieldTypeGroupMetric(actual_type,
                              AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
      KMaxFieldTypeGroupMetric);
  LogUMAHistogramEnumeration(
      type_specific_histogram,
      GetFieldTypeGroupMetric(predicted_type,
                              AutofillMetrics::FALSE_POSITIVE_MISMATCH),
      KMaxFieldTypeGroupMetric);
}

// Logs field type prediction quality metrics.  The primary histogram name is
// constructed based on |prediction_source| The field-specific histogram names
// also incorporates the possible and predicted types for |field|. A suffix may
// be appended to the metric name, depending on |metric_type|.
void LogPredictionQualityMetrics(
    AutofillMetrics::QualityMetricPredictionSource prediction_source,
    ServerFieldType predicted_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    AutofillMetrics::QualityMetricType metric_type,
    bool log_rationalization_metrics) {
  // Generate histogram names.
  const char* source = GetQualityMetricPredictionSource(prediction_source);
  const char* suffix = GetQualityMetricTypeSuffix(metric_type);
  std::string raw_data_histogram =
      base::JoinString({"Autofill.FieldPrediction.", source, suffix}, "");
  std::string aggregate_histogram = base::JoinString(
      {"Autofill.FieldPredictionQuality.Aggregate.", source, suffix}, "");
  std::string type_specific_histogram = base::JoinString(
      {"Autofill.FieldPredictionQuality.ByFieldType.", source, suffix}, "");
  std::string rationalization_quality_histogram = base::JoinString(
      {"Autofill.RationalizationQuality.PhoneNumber", suffix}, "");

  const ServerFieldTypeSet& possible_types =
      metric_type == AutofillMetrics::TYPE_AUTOCOMPLETE_BASED
          ? ServerFieldTypeSet{AutofillType(field.html_type(),
                                            field.html_mode())
                                   .GetStorableType()}
          : field.possible_types();

  // Get the best type classification we can for the field.
  ServerFieldType actual_type =
      GetActualFieldType(possible_types, predicted_type);

  DVLOG(2) << "Predicted: " << AutofillType(predicted_type).ToString() << "; "
           << "Actual: " << AutofillType(actual_type).ToString();

  DCHECK_LE(predicted_type, UINT16_MAX);
  DCHECK_LE(actual_type, UINT16_MAX);
  base::UmaHistogramSparse(raw_data_histogram,
                           (predicted_type << 16) | actual_type);

  form_interactions_ukm_logger->LogFieldType(
      form.form_parsed_timestamp(), form.form_signature(),
      field.GetFieldSignature(), prediction_source, metric_type, predicted_type,
      actual_type);

  // NO_SERVER_DATA is the equivalent of predicting UNKNOWN.
  if (predicted_type == NO_SERVER_DATA)
    predicted_type = UNKNOWN_TYPE;

  // The actual type being EMPTY_TYPE is the same as UNKNOWN_TYPE for comparison
  // purposes, but remember whether or not it was empty for more precise logging
  // later.
  bool is_empty = (actual_type == EMPTY_TYPE);
  bool is_ambiguous = (actual_type == AMBIGUOUS_TYPE);
  if (is_empty || is_ambiguous)
    actual_type = UNKNOWN_TYPE;

  // Log metrics for a field that is |only_fill_when_focused|==true. Basically
  // autofill might have a field prediction but it also thinks it should not
  // be filled automatically unless user focused on the field. This requires
  // different metrics logging than normal fields.
  if (field.only_fill_when_focused()) {
    LogPredictionQualityMetricsForFieldsOnlyFilledWhenFocused(
        aggregate_histogram, type_specific_histogram,
        rationalization_quality_histogram, predicted_type, actual_type,
        is_empty, is_ambiguous, log_rationalization_metrics, form, field);
    return;
  }

  LogPredictionQualityMetricsForCommonFields(
      aggregate_histogram, type_specific_histogram, predicted_type, actual_type,
      is_empty, is_ambiguous);
}

AutofillMetrics::FormEvent GetCardNumberStatusFormEvent(
    const AutofillMetrics::CardNumberStatus card_number_status) {
  switch (card_number_status) {
    case AutofillMetrics::EMPTY_CARD:
      return AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD;
    case AutofillMetrics::WRONG_SIZE_CARD:
      return AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_WRONG_SIZE_CARD;
    case AutofillMetrics::FAIL_LUHN_CHECK_CARD:
      return AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_FAIL_LUHN_CHECK_CARD;
    case AutofillMetrics::KNOWN_CARD:
      return AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD;
    case AutofillMetrics::UNKNOWN_CARD:
      return AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD;
  }
  NOTREACHED();
  return AutofillMetrics::
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD;
}

}  // namespace

// static
void AutofillMetrics::UpdateSourceURL(ukm::UkmRecorder* ukm_recorder,
                                      ukm::SourceId source_id,
                                      const GURL& url) {
  if (ukm_recorder)
    ukm_recorder->UpdateSourceURL(source_id, url);
}

// static
void AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
    SubmittedServerCardExpirationStatusMetric metric) {
  DCHECK_LT(metric, NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS);
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.SubmittedServerCardExpirationStatus", metric,
      NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS);
}

// static
void AutofillMetrics::LogCardUploadDecisionMetrics(
    int upload_decision_metrics) {
  DCHECK(upload_decision_metrics);
  DCHECK_LT(upload_decision_metrics, 1 << kNumCardUploadDecisionMetrics);

  for (int metric = 0; metric < kNumCardUploadDecisionMetrics; ++metric)
    if (upload_decision_metrics & (1 << metric))
      UMA_HISTOGRAM_ENUMERATION("Autofill.CardUploadDecisionMetric", metric,
                                kNumCardUploadDecisionMetrics);
}

// static
void AutofillMetrics::LogCreditCardInfoBarMetric(
    InfoBarMetric metric,
    bool is_uploading,
    int previous_save_credit_card_prompt_user_decision) {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);

  std::string destination = is_uploading ? ".Server" : ".Local";
  LogUMAHistogramEnumeration(
      "Autofill.CreditCardInfoBar" + destination +
          PreviousSaveCreditCardPromptUserDecisionToString(
              previous_save_credit_card_prompt_user_decision),
      metric, NUM_INFO_BAR_METRICS);
}

// static
void AutofillMetrics::LogCreditCardFillingInfoBarMetric(InfoBarMetric metric) {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardFillingInfoBar", metric,
                            NUM_INFO_BAR_METRICS);
}

// static
void AutofillMetrics::LogSaveCardPromptMetric(
    SaveCardPromptMetric metric,
    bool is_uploading,
    bool is_reshow,
    int previous_save_credit_card_prompt_user_decision) {
  DCHECK_LT(metric, NUM_SAVE_CARD_PROMPT_METRICS);
  std::string destination = is_uploading ? ".Upload" : ".Local";
  std::string show = is_reshow ? ".Reshows" : ".FirstShow";
  LogUMAHistogramEnumeration(
      "Autofill.SaveCreditCardPrompt" + destination + show +
          PreviousSaveCreditCardPromptUserDecisionToString(
              previous_save_credit_card_prompt_user_decision),
      metric, NUM_SAVE_CARD_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardPromptMetric(
    ScanCreditCardPromptMetric metric) {
  DCHECK_LT(metric, NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ScanCreditCardPrompt", metric,
                            NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardCompleted(
    const base::TimeDelta& duration,
    bool completed) {
  std::string suffix = completed ? "Completed" : "Cancelled";
  LogUMAHistogramLongTimes("Autofill.ScanCreditCard.Duration_" + suffix,
                           duration);
  UMA_HISTOGRAM_BOOLEAN("Autofill.ScanCreditCard.Completed", completed);
}

// static
void AutofillMetrics::LogUnmaskPromptEvent(UnmaskPromptEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.UnmaskPrompt.Events", event,
                            NUM_UNMASK_PROMPT_EVENTS);
}

// static
void AutofillMetrics::LogUnmaskPromptEventDuration(
    const base::TimeDelta& duration,
    UnmaskPromptEvent close_event) {
  std::string suffix;
  switch (close_event) {
    case UNMASK_PROMPT_CLOSED_NO_ATTEMPTS:
      suffix = "NoAttempts";
      break;
    case UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE:
    case UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE:
      suffix = "Failure";
      break;
    case UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING:
      suffix = "AbandonUnmasking";
      break;
    case UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT:
    case UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS:
      suffix = "Success";
      break;
    default:
      NOTREACHED();
      return;
  }
  LogUMAHistogramLongTimes("Autofill.UnmaskPrompt.Duration", duration);
  LogUMAHistogramLongTimes("Autofill.UnmaskPrompt.Duration." + suffix,
                           duration);
}

// static
void AutofillMetrics::LogTimeBeforeAbandonUnmasking(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_LONG_TIMES("Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking",
                           duration);
}

// static
void AutofillMetrics::LogRealPanResult(
    AutofillClient::PaymentsRpcResult result) {
  PaymentsRpcResult metric_result;
  switch (result) {
    case AutofillClient::SUCCESS:
      metric_result = PAYMENTS_RESULT_SUCCESS;
      break;
    case AutofillClient::TRY_AGAIN_FAILURE:
      metric_result = PAYMENTS_RESULT_TRY_AGAIN_FAILURE;
      break;
    case AutofillClient::PERMANENT_FAILURE:
      metric_result = PAYMENTS_RESULT_PERMANENT_FAILURE;
      break;
    case AutofillClient::NETWORK_ERROR:
      metric_result = PAYMENTS_RESULT_NETWORK_ERROR;
      break;
    default:
      NOTREACHED();
      return;
  }
  UMA_HISTOGRAM_ENUMERATION("Autofill.UnmaskPrompt.GetRealPanResult",
                            metric_result, NUM_PAYMENTS_RESULTS);
}

// static
void AutofillMetrics::LogRealPanDuration(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result) {
  std::string suffix;
  switch (result) {
    case AutofillClient::SUCCESS:
      suffix = "Success";
      break;
    case AutofillClient::TRY_AGAIN_FAILURE:
    case AutofillClient::PERMANENT_FAILURE:
      suffix = "Failure";
      break;
    case AutofillClient::NETWORK_ERROR:
      suffix = "NetworkError";
      break;
    default:
      NOTREACHED();
      return;
  }
  LogUMAHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration",
                           duration);
  LogUMAHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration." + suffix,
                           duration);
}

// static
void AutofillMetrics::LogUnmaskingDuration(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result) {
  std::string suffix;
  switch (result) {
    case AutofillClient::SUCCESS:
      suffix = "Success";
      break;
    case AutofillClient::TRY_AGAIN_FAILURE:
    case AutofillClient::PERMANENT_FAILURE:
      suffix = "Failure";
      break;
    case AutofillClient::NETWORK_ERROR:
      suffix = "NetworkError";
      break;
    default:
      NOTREACHED();
      return;
  }
  LogUMAHistogramLongTimes("Autofill.UnmaskPrompt.UnmaskingDuration", duration);
  LogUMAHistogramLongTimes("Autofill.UnmaskPrompt.UnmaskingDuration." + suffix,
                           duration);
}

// static
void AutofillMetrics::LogDeveloperEngagementMetric(
    DeveloperEngagementMetric metric) {
  DCHECK_LT(metric, NUM_DEVELOPER_ENGAGEMENT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.DeveloperEngagement", metric,
                            NUM_DEVELOPER_ENGAGEMENT_METRICS);
}

void AutofillMetrics::LogHeuristicPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_HEURISTIC,
      AutofillType(field.heuristic_type()).GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      false /*log_rationalization_metrics*/);
}

void AutofillMetrics::LogServerPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_SERVER,
      AutofillType(field.overall_server_type()).GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      false /*log_rationalization_metrics*/);
}

void AutofillMetrics::LogOverallPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_OVERALL, field.Type().GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      true /*log_rationalization_metrics*/);
}

// static
void AutofillMetrics::LogServerQueryMetric(ServerQueryMetric metric) {
  DCHECK_LT(metric, NUM_SERVER_QUERY_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

// static
void AutofillMetrics::LogUserHappinessMetric(UserHappinessMetric metric,
                                             FieldTypeGroup field_type_group) {
  LogUserHappinessMetric(
      metric, {FormTypes::FieldTypeGroupToFormType(field_type_group)});
}

// static
void AutofillMetrics::LogUserHappinessMetric(
    UserHappinessMetric metric,
    const std::set<FormType>& form_types) {
  DCHECK_LT(metric, NUM_USER_HAPPINESS_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness", metric,
                            NUM_USER_HAPPINESS_METRICS);
  if (base::ContainsKey(form_types, CREDIT_CARD_FORM)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.CreditCard", metric,
                              NUM_USER_HAPPINESS_METRICS);
  }
  if (base::ContainsKey(form_types, ADDRESS_FORM)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Address", metric,
                              NUM_USER_HAPPINESS_METRICS);
  }
  if (base::ContainsKey(form_types, PASSWORD_FORM)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Password", metric,
                              NUM_USER_HAPPINESS_METRICS);
  }
  if (base::ContainsKey(form_types, UNKNOWN_FORM_TYPE)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Unknown", metric,
                              NUM_USER_HAPPINESS_METRICS);
  }
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(
    const base::TimeDelta& duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithAutofill", duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(
    const base::TimeDelta& duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithoutAutofill",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromInteraction(
    const std::set<FormType>& form_types,
    bool used_autofill,
    const base::TimeDelta& duration) {
  std::string parent_metric;
  if (used_autofill) {
    parent_metric = "Autofill.FillDuration.FromInteraction.WithAutofill";
  } else {
    parent_metric = "Autofill.FillDuration.FromInteraction.WithoutAutofill";
  }
  LogFormFillDuration(parent_metric, duration);
  if (base::ContainsKey(form_types, CREDIT_CARD_FORM)) {
    LogFormFillDuration(parent_metric + ".CreditCard", duration);
  }
  if (base::ContainsKey(form_types, ADDRESS_FORM)) {
    LogFormFillDuration(parent_metric + ".Address", duration);
  }
  if (base::ContainsKey(form_types, PASSWORD_FORM)) {
    LogFormFillDuration(parent_metric + ".Password", duration);
  }
  if (base::ContainsKey(form_types, UNKNOWN_FORM_TYPE)) {
    LogFormFillDuration(parent_metric + ".Unknown", duration);
  }
}

// static
void AutofillMetrics::LogFormFillDuration(const std::string& metric,
                                          const base::TimeDelta& duration) {
  base::UmaHistogramCustomTimes(metric, duration,
                                base::TimeDelta::FromMilliseconds(100),
                                base::TimeDelta::FromMinutes(10), 50);
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
void AutofillMetrics::LogStoredProfileDisusedCount(size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredProfileDisusedCount", num_profiles);
}

// static
void AutofillMetrics::LogStoredProfileDaysSinceLastUse(size_t days) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.StoredProfile", days);
}

// static
void AutofillMetrics::LogStoredCreditCardMetrics(
    const std::vector<std::unique_ptr<CreditCard>>& local_cards,
    const std::vector<std::unique_ptr<CreditCard>>& server_cards,
    base::TimeDelta disused_data_threshold) {
  size_t num_local_cards = 0;
  size_t num_masked_cards = 0;
  size_t num_unmasked_cards = 0;
  size_t num_disused_local_cards = 0;
  size_t num_disused_masked_cards = 0;
  size_t num_disused_unmasked_cards = 0;

  // Concatenate the local and server cards into one big collection of raw
  // CreditCard pointers.
  std::vector<const CreditCard*> credit_cards;
  credit_cards.reserve(local_cards.size() + server_cards.size());
  for (const auto* collection : {&local_cards, &server_cards}) {
    for (const auto& card : *collection) {
      credit_cards.push_back(card.get());
    }
  }

  // Iterate over all of the cards and gather metrics.
  const base::Time now = AutofillClock::Now();
  for (const CreditCard* card : credit_cards) {
    const base::TimeDelta time_since_last_use = now - card->use_date();
    const int days_since_last_use = time_since_last_use.InDays();
    const int disused_delta =
        (time_since_last_use > disused_data_threshold) ? 1 : 0;
    UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.StoredCreditCard",
                              days_since_last_use);
    switch (card->record_type()) {
      case CreditCard::LOCAL_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Local",
            days_since_last_use);
        num_local_cards += 1;
        num_disused_local_cards += disused_delta;
        break;
      case CreditCard::MASKED_SERVER_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server",
            days_since_last_use);
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked",
            days_since_last_use);
        num_masked_cards += 1;
        num_disused_masked_cards += disused_delta;
        break;
      case CreditCard::FULL_SERVER_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server",
            days_since_last_use);
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked",
            days_since_last_use);
        num_unmasked_cards += 1;
        num_disused_unmasked_cards += disused_delta;
        break;
    }
  }

  // Calculate some summary info.
  const size_t num_server_cards = num_masked_cards + num_unmasked_cards;
  const size_t num_cards = num_local_cards + num_server_cards;
  const size_t num_disused_server_cards =
      num_disused_masked_cards + num_disused_unmasked_cards;
  const size_t num_disused_cards =
      num_disused_local_cards + num_disused_server_cards;

  // Log the overall counts.
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount", num_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local",
                            num_local_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server",
                            num_server_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server.Masked",
                            num_masked_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server.Unmasked",
                            num_unmasked_cards);

  // For card types held by the user, log how many are disused.
  if (num_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount",
                              num_disused_cards);
  }
  if (num_local_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount.Local",
                              num_disused_local_cards);
  }
  if (num_server_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount.Server",
                              num_disused_server_cards);
  }
  if (num_masked_cards) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.StoredCreditCardDisusedCount.Server.Masked",
        num_disused_masked_cards);
  }
  if (num_unmasked_cards) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.StoredCreditCardDisusedCount.Server.Unmasked",
        num_disused_unmasked_cards);
  }

  // Legacy histogram names.
  // Validated by:
  //     AutofillMetricsTest.StoredLocalCreditCardCount
  //     AutofillMetricsTest.StoredServerCreditCardCount_Masked
  //     AutofillMetricsTest.StoredServerCreditCardCount_Unmasked
  // TODO(crbug/762131): Delete these in 2018/Q2 once enough UMA history is
  // established for the new names.
  UMA_HISTOGRAM_COUNTS("Autofill.StoredLocalCreditCardCount", num_local_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredServerCreditCardCount.Masked",
                            num_masked_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredServerCreditCardCount.Unmasked",
                            num_unmasked_cards);
}

// static
void AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
    size_t num_cards) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.CreditCardsSuppressedForDisuse",
                            num_cards);
}

// static
void AutofillMetrics::LogNumberOfCreditCardsDeletedForDisuse(size_t num_cards) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.CreditCardsDeletedForDisuse", num_cards);
}

// static
void AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", num_profiles);
}

// static
void AutofillMetrics::LogHasModifiedProfileOnCreditCardFormSubmission(
    bool has_modified_profile) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.HasModifiedProfile.CreditCardFormSubmission",
                        has_modified_profile);
}

// static
void AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.AddressesSuppressedForDisuse",
                            num_profiles);
}

// static
void AutofillMetrics::LogNumberOfAddressesDeletedForDisuse(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.AddressesDeletedForDisuse", num_profiles);
}

// static
void AutofillMetrics::LogAddressSuggestionsCount(size_t num_suggestions) {
  UMA_HISTOGRAM_COUNTS("Autofill.AddressSuggestionsCount", num_suggestions);
}

// static
void AutofillMetrics::LogAutofillSuggestionAcceptedIndex(int index) {
  // A maximum of 50 is enforced to minimize the number of buckets generated.
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex",
                           std::min(index, 50));

  base::RecordAction(base::UserMetricsAction("Autofill_SelectedSuggestion"));
}

// static
void AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(int index) {
  // A maximum of 50 is enforced to minimize the number of buckets generated.
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Autocomplete",
                           std::min(index, 50));
}

// static
void AutofillMetrics::LogNumberOfEditedAutofilledFields(
    size_t num_edited_autofilled_fields,
    bool observed_submission) {
  if (observed_submission) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.NumberOfEditedAutofilledFieldsAtSubmission",
        num_edited_autofilled_fields);
  } else {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.NumberOfEditedAutofilledFieldsAtSubmission.NoSubmission",
        num_edited_autofilled_fields);
  }
}

// static
void AutofillMetrics::LogServerResponseHasDataForForm(bool has_data) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ServerResponseHasDataForForm", has_data);
}

// static
void AutofillMetrics::LogProfileActionOnFormSubmitted(
    AutofillProfileAction action) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.ProfileActionOnFormSubmitted", action,
                            AUTOFILL_PROFILE_ACTION_ENUM_SIZE);
}

// static
void AutofillMetrics::LogAutofillFormSubmittedState(
    AutofillFormSubmittedState state,
    const base::TimeTicks& form_parsed_timestamp,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.FormSubmittedState", state,
                            AUTOFILL_FORM_SUBMITTED_STATE_ENUM_SIZE);

  switch (state) {
    case NON_FILLABLE_FORM_OR_NEW_DATA:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_NonFillable"));
      break;

    case FILLABLE_FORM_AUTOFILLED_ALL:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_FilledAll"));
      break;

    case FILLABLE_FORM_AUTOFILLED_SOME:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_FilledSome"));
      break;

    case FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS:
      base::RecordAction(base::UserMetricsAction(
          "Autofill_FormSubmitted_FilledNone_SuggestionsShown"));
      break;

    case FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS:
      base::RecordAction(base::UserMetricsAction(
          "Autofill_FormSubmitted_FilledNone_SuggestionsNotShown"));
      break;

    default:
      NOTREACHED();
      break;
  }
  form_interactions_ukm_logger->LogFormSubmitted(state, form_parsed_timestamp);
}

// static
void AutofillMetrics::LogDetermineHeuristicTypesTiming(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("Autofill.Timing.DetermineHeuristicTypes", duration);
}

// static
void AutofillMetrics::LogParseFormTiming(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("Autofill.Timing.ParseForm", duration);
}

// static
void AutofillMetrics::LogNumberOfProfilesConsideredForDedupe(
    size_t num_considered) {
  // A maximum of 50 is enforced to reduce the number of generated buckets.
  UMA_HISTOGRAM_COUNTS_1000("Autofill.NumberOfProfilesConsideredForDedupe",
                            std::min(int(num_considered), 50));
}

// static
void AutofillMetrics::LogNumberOfProfilesRemovedDuringDedupe(
    size_t num_removed) {
  // A maximum of 50 is enforced to reduce the number of generated buckets.
  UMA_HISTOGRAM_COUNTS_1000("Autofill.NumberOfProfilesRemovedDuringDedupe",
                            std::min(int(num_removed), 50));
}

// static
void AutofillMetrics::LogIsQueriedCreditCardFormSecure(bool is_secure) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.QueriedCreditCardFormIsSecure", is_secure);
}

// static
void AutofillMetrics::LogWalletAddressConversionType(
    WalletAddressConversionType type) {
  DCHECK_LT(type, NUM_CONVERTED_ADDRESS_CONVERSION_TYPES);
  UMA_HISTOGRAM_ENUMERATION("Autofill.WalletAddressConversionType", type,
                            NUM_CONVERTED_ADDRESS_CONVERSION_TYPES);
}

// static
void AutofillMetrics::LogShowedHttpNotSecureExplanation() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedHttpNotSecureExplanation"));
}

// static
void AutofillMetrics::LogCardUploadDecisionsUkm(ukm::UkmRecorder* ukm_recorder,
                                                const GURL& url,
                                                int upload_decision_metrics) {
  DCHECK(upload_decision_metrics);
  DCHECK_LT(upload_decision_metrics, 1 << kNumCardUploadDecisionMetrics);
  if (!url.is_valid())
    return;
  ukm::SourceId source_id = NewUkmSourceWithUrl(ukm_recorder, url);
  ukm::builders::Autofill_CardUploadDecision(source_id)
      .SetUploadDecision(upload_decision_metrics)
      .Record(ukm_recorder);
}

// static
void AutofillMetrics::LogDeveloperEngagementUkm(
    ukm::UkmRecorder* ukm_recorder,
    const GURL& url,
    int developer_engagement_metrics) {
  DCHECK(developer_engagement_metrics);
  DCHECK_LT(developer_engagement_metrics,
            1 << NUM_DEVELOPER_ENGAGEMENT_METRICS);
  if (!url.is_valid())
    return;
  ukm::SourceId source_id = NewUkmSourceWithUrl(ukm_recorder, url);
  ukm::builders::Autofill_DeveloperEngagement(source_id)
      .SetDeveloperEngagement(developer_engagement_metrics)
      .Record(ukm_recorder);
}

AutofillMetrics::FormEventLogger::FormEventLogger(
    bool is_for_credit_card,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger)
    : is_for_credit_card_(is_for_credit_card),
      server_record_type_count_(0),
      local_record_type_count_(0),
      is_context_secure_(false),
      has_logged_interacted_(false),
      has_logged_suggestions_shown_(false),
      has_logged_masked_server_card_suggestion_selected_(false),
      has_logged_suggestion_filled_(false),
      has_logged_will_submit_(false),
      has_logged_submitted_(false),
      has_logged_bank_name_available_(false),
      logged_suggestion_filled_was_server_data_(false),
      logged_suggestion_filled_was_masked_server_card_(false),
      form_interactions_ukm_logger_(form_interactions_ukm_logger) {}

void AutofillMetrics::FormEventLogger::OnDidInteractWithAutofillableForm() {
  if (!has_logged_interacted_) {
    has_logged_interacted_ = true;
    form_interactions_ukm_logger_->LogInteractedWithForm(
        is_for_credit_card_, local_record_type_count_,
        server_record_type_count_);
    Log(AutofillMetrics::FORM_EVENT_INTERACTED_ONCE);
  }
}

void AutofillMetrics::FormEventLogger::OnDidPollSuggestions(
    const FormFieldData& field) {
  // Record only one poll user action for consecutive polls of the same field.
  // This is to avoid recording too many poll actions (for example when a user
  // types in a field, triggering multiple queries) to make the analysis more
  // simple.
  if (!field.SameFieldAs(last_polled_field_)) {
    if (is_for_credit_card_) {
      base::RecordAction(
          base::UserMetricsAction("Autofill_PolledCreditCardSuggestions"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("Autofill_PolledProfileSuggestions"));
    }

    last_polled_field_ = field;
  }
}

void AutofillMetrics::FormEventLogger::OnDidShowSuggestions(
    const AutofillField& field,
    const base::TimeTicks& form_parsed_timestamp) {
  form_interactions_ukm_logger_->LogSuggestionsShown(field,
                                                     form_parsed_timestamp);

  Log(AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN);
  if (!has_logged_suggestions_shown_) {
    has_logged_suggestions_shown_ = true;
    Log(AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE);
    if (has_logged_bank_name_available_) {
      Log(AutofillMetrics::
              FORM_EVENT_SUGGESTIONS_SHOWN_WITH_BANK_NAME_AVAILABLE_ONCE);
    }
  }

  if (is_for_credit_card_) {
    base::RecordAction(
        base::UserMetricsAction("Autofill_ShowedCreditCardSuggestions"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Autofill_ShowedProfileSuggestions"));
  }
}

void AutofillMetrics::FormEventLogger::OnDidSelectMaskedServerCardSuggestion(
    const base::TimeTicks& form_parsed_timestamp) {
  DCHECK(is_for_credit_card_);
  form_interactions_ukm_logger_->LogSelectedMaskedServerCard(
      form_parsed_timestamp);

  Log(AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED);
  if (!has_logged_masked_server_card_suggestion_selected_) {
    has_logged_masked_server_card_suggestion_selected_ = true;
    Log(AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE);
  }
}

void AutofillMetrics::FormEventLogger::OnDidFillSuggestion(
    const CreditCard& credit_card,
    const base::TimeTicks& form_parsed_timestamp) {
  DCHECK(is_for_credit_card_);
  form_interactions_ukm_logger_->LogDidFillSuggestion(
      static_cast<int>(credit_card.record_type()), form_parsed_timestamp);

  if (credit_card.record_type() == CreditCard::MASKED_SERVER_CARD)
    Log(AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED);
  else if (credit_card.record_type() == CreditCard::FULL_SERVER_CARD)
    Log(AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED);
  else
    Log(AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED);

  if (!has_logged_suggestion_filled_) {
    has_logged_suggestion_filled_ = true;
    logged_suggestion_filled_was_server_data_ =
        credit_card.record_type() == CreditCard::MASKED_SERVER_CARD ||
        credit_card.record_type() == CreditCard::FULL_SERVER_CARD;
    logged_suggestion_filled_was_masked_server_card_ =
        credit_card.record_type() == CreditCard::MASKED_SERVER_CARD;
    if (credit_card.record_type() == CreditCard::MASKED_SERVER_CARD) {
      Log(AutofillMetrics::
              FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE);
    } else if (credit_card.record_type() == CreditCard::FULL_SERVER_CARD) {
      Log(AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE);
      if (has_logged_bank_name_available_) {
        Log(AutofillMetrics::
                FORM_EVENT_SERVER_SUGGESTION_FILLED_WITH_BANK_NAME_AVAILABLE_ONCE);
      }
    } else {
      Log(AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE);
    }
  }

  base::RecordAction(
      base::UserMetricsAction("Autofill_FilledCreditCardSuggestion"));
}

void AutofillMetrics::FormEventLogger::OnDidFillSuggestion(
    const AutofillProfile& profile,
    const base::TimeTicks& form_parsed_timestamp) {
  DCHECK(!is_for_credit_card_);
  form_interactions_ukm_logger_->LogDidFillSuggestion(
      static_cast<int>(profile.record_type()), form_parsed_timestamp);

  if (profile.record_type() == AutofillProfile::SERVER_PROFILE)
    Log(AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED);
  else
    Log(AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED);

  if (!has_logged_suggestion_filled_) {
    has_logged_suggestion_filled_ = true;
    logged_suggestion_filled_was_server_data_ =
        profile.record_type() == AutofillProfile::SERVER_PROFILE;
    Log(profile.record_type() == AutofillProfile::SERVER_PROFILE
            ? AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE
            : AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE);
  }

  base::RecordAction(
      base::UserMetricsAction("Autofill_FilledProfileSuggestion"));
}

void AutofillMetrics::FormEventLogger::OnWillSubmitForm() {
  // Not logging this kind of form if we haven't logged a user interaction.
  if (!has_logged_interacted_)
    return;

  // Not logging twice.
  if (has_logged_will_submit_)
    return;
  has_logged_will_submit_ = true;

  if (!has_logged_suggestion_filled_) {
    Log(AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE);
  } else if (logged_suggestion_filled_was_masked_server_card_) {
    Log(AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE);
  } else {
    Log(AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE);
  }

  if (has_logged_suggestions_shown_) {
    Log(AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE);
  }

  base::RecordAction(base::UserMetricsAction("Autofill_OnWillSubmitForm"));
}

void AutofillMetrics::FormEventLogger::OnFormSubmitted(
    bool force_logging,
    CardNumberStatus card_number_status) {
  // Not logging this kind of form if we haven't logged a user interaction.
  if (!has_logged_interacted_)
    return;

  // Not logging twice.
  if (has_logged_submitted_)
    return;
  has_logged_submitted_ = true;

  if (!has_logged_suggestion_filled_) {
    Log(AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE);
  } else if (logged_suggestion_filled_was_masked_server_card_) {
    Log(AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE);
  } else if (logged_suggestion_filled_was_server_data_) {
    Log(AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE);
  } else {
    Log(AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE);
  }

  if (has_logged_suggestions_shown_ || force_logging) {
    Log(AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE);
    if (is_for_credit_card_ && !has_logged_suggestion_filled_) {
      Log(GetCardNumberStatusFormEvent(card_number_status));
    }
  }
}

void AutofillMetrics::FormEventLogger::SetBankNameAvailable() {
  has_logged_bank_name_available_ = true;
}

void AutofillMetrics::FormEventLogger::Log(FormEvent event) const {
  DCHECK_LT(event, NUM_FORM_EVENTS);
  std::string name("Autofill.FormEvents.");
  if (is_for_credit_card_)
    name += "CreditCard";
  else
    name += "Address";
  LogUMAHistogramEnumeration(name, event, NUM_FORM_EVENTS);

  // Log again in a different histogram for credit card forms on nonsecure
  // pages, so that form interactions on nonsecure pages can be analyzed on
  // their own.
  if (is_for_credit_card_ && !is_context_secure_) {
    LogUMAHistogramEnumeration(name + ".OnNonsecurePage", event,
                               NUM_FORM_EVENTS);
  }

  // Logging again in a different histogram for segmentation purposes.
  // TODO(waltercacau): Re-evaluate if we still need such fine grained
  // segmentation. http://crbug.com/454018
  if (server_record_type_count_ == 0 && local_record_type_count_ == 0)
    name += ".WithNoData";
  else if (server_record_type_count_ > 0 && local_record_type_count_ == 0)
    name += ".WithOnlyServerData";
  else if (server_record_type_count_ == 0 && local_record_type_count_ > 0)
    name += ".WithOnlyLocalData";
  else
    name += ".WithBothServerAndLocalData";
  LogUMAHistogramEnumeration(name, event, NUM_FORM_EVENTS);
}

void AutofillMetrics::FormEventLogger::Log(
    BankNameDisplayedFormEvent event) const {
  DCHECK_LT(event, BANK_NAME_NUM_FORM_EVENTS);
  std::string name("Autofill.FormEvents.CreditCard.BankNameDisplayed");
  LogUMAHistogramEnumeration(name, event, BANK_NAME_NUM_FORM_EVENTS);
}

AutofillMetrics::FormInteractionsUkmLogger::FormInteractionsUkmLogger(
    ukm::UkmRecorder* ukm_recorder)
    : ukm_recorder_(ukm_recorder) {}

void AutofillMetrics::FormInteractionsUkmLogger::OnFormsParsed(
    const GURL& url) {
  if (ukm_recorder_ == nullptr)
    return;

  url_ = url;
}

void AutofillMetrics::FormInteractionsUkmLogger::LogInteractedWithForm(
    bool is_for_credit_card,
    size_t local_record_type_count,
    size_t server_record_type_count) {
  if (!CanLog())
    return;

  if (source_id_ == -1)
    GetNewSourceID();

  ukm::builders::Autofill_InteractedWithForm(source_id_)
      .SetIsForCreditCard(is_for_credit_card)
      .SetLocalRecordTypeCount(local_record_type_count)
      .SetServerRecordTypeCount(server_record_type_count)
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogSuggestionsShown(
    const AutofillField& field,
    const base::TimeTicks& form_parsed_timestamp) {
  if (!CanLog())
    return;

  if (source_id_ == -1)
    GetNewSourceID();

  ukm::builders::Autofill_SuggestionsShown(source_id_)
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetServerType(static_cast<int>(field.overall_server_type()))
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogSelectedMaskedServerCard(
    const base::TimeTicks& form_parsed_timestamp) {
  if (!CanLog())
    return;

  if (source_id_ == -1)
    GetNewSourceID();

  ukm::builders::Autofill_SelectedMaskedServerCard(source_id_)
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogDidFillSuggestion(
    int record_type,
    const base::TimeTicks& form_parsed_timestamp) {
  if (!CanLog())
    return;

  if (source_id_ == -1)
    GetNewSourceID();

  ukm::builders::Autofill_SuggestionFilled(source_id_)
      .SetRecordType(record_type)
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogTextFieldDidChange(
    const AutofillField& field,
    const base::TimeTicks& form_parsed_timestamp) {
  if (!CanLog())
    return;

  if (source_id_ == -1)
    GetNewSourceID();

  ukm::builders::Autofill_TextFieldDidChange(source_id_)
      .SetFieldTypeGroup(static_cast<int>(field.Type().group()))
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetServerType(static_cast<int>(field.overall_server_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetHtmlFieldMode(static_cast<int>(field.html_mode()))
      .SetIsAutofilled(field.is_autofilled)
      .SetIsEmpty(field.IsEmpty())
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogFieldFillStatus(
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  if (!CanLog())
    return;

  if (source_id_ == -1)
    GetNewSourceID();

  ukm::builders::Autofill_FieldFillStatus(source_id_)
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form.form_parsed_timestamp()))
      .SetFormSignature(static_cast<int64_t>(form.form_signature()))
      .SetFieldSignature(static_cast<int64_t>(field.GetFieldSignature()))
      .SetValidationEvent(static_cast<int64_t>(metric_type))
      .SetIsAutofilled(static_cast<int64_t>(field.is_autofilled))
      .SetWasPreviouslyAutofilled(
          static_cast<int64_t>(field.previously_autofilled()))
      .Record(ukm_recorder_);
}

// TODO(szhangcs): Take FormStructure and AutofillField and extract
// FormSignature and TimeTicks inside the function.
void AutofillMetrics::FormInteractionsUkmLogger::LogFieldType(
    const base::TimeTicks& form_parsed_timestamp,
    FormSignature form_signature,
    FieldSignature field_signature,
    QualityMetricPredictionSource prediction_source,
    QualityMetricType metric_type,
    ServerFieldType predicted_type,
    ServerFieldType actual_type) {
  if (!CanLog())
    return;

  if (source_id_ == -1)
    GetNewSourceID();

  ukm::builders::Autofill_FieldTypeValidation(source_id_)
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .SetFormSignature(static_cast<int64_t>(form_signature))
      .SetFieldSignature(static_cast<int64_t>(field_signature))
      .SetValidationEvent(static_cast<int64_t>(metric_type))
      .SetPredictionSource(static_cast<int64_t>(prediction_source))
      .SetPredictedType(static_cast<int64_t>(predicted_type))
      .SetActualType(static_cast<int64_t>(actual_type))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogFormSubmitted(
    AutofillFormSubmittedState state,
    const base::TimeTicks& form_parsed_timestamp) {
  if (!CanLog())
    return;

  if (source_id_ == -1)
    GetNewSourceID();

  ukm::builders::Autofill_FormSubmitted builder(source_id_);
  builder.SetAutofillFormSubmittedState(static_cast<int>(state));
  if (form_parsed_timestamp.is_null())
    DCHECK(state == NON_FILLABLE_FORM_OR_NEW_DATA ||
           state == FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS)
        << state;
  else
    builder.SetMillisecondsSinceFormParsed(
        MillisecondsSinceFormParsed(form_parsed_timestamp));

  builder.Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::UpdateSourceURL(
    const GURL& url) {
  url_ = url;
  if (CanLog())
    AutofillMetrics::UpdateSourceURL(ukm_recorder_, source_id_, url_);
}

bool AutofillMetrics::FormInteractionsUkmLogger::CanLog() const {
  return ukm_recorder_ && url_.is_valid();
}

int64_t AutofillMetrics::FormInteractionsUkmLogger::MillisecondsSinceFormParsed(
    const base::TimeTicks& form_parsed_timestamp) const {
  DCHECK(!form_parsed_timestamp.is_null());
  // Use the pinned timestamp as the current time if it's set.
  base::TimeTicks now =
      pinned_timestamp_.is_null() ? base::TimeTicks::Now() : pinned_timestamp_;
  return (now - form_parsed_timestamp).InMilliseconds();
}

void AutofillMetrics::FormInteractionsUkmLogger::GetNewSourceID() {
  source_id_ = ukm_recorder_->GetNewSourceID();
  AutofillMetrics::UpdateSourceURL(ukm_recorder_, source_id_, url_);
}

AutofillMetrics::UkmTimestampPin::UkmTimestampPin(
    FormInteractionsUkmLogger* logger)
    : logger_(logger) {
  DCHECK(logger_);
  DCHECK(!logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(base::TimeTicks::Now());
}

AutofillMetrics::UkmTimestampPin::~UkmTimestampPin() {
  DCHECK(logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(base::TimeTicks());
}

}  // namespace autofill
