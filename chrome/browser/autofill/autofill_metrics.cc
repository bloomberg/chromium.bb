// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/autofill/autofill_type.h"

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
  FAX,
  EMAIL,
  CREDIT_CARD_NAME,
  CREDIT_CARD_NUMBER,
  CREDIT_CARD_DATE,
  NUM_FIELD_TYPE_GROUPS_FOR_METRICS
};

// Translates |field_type| to the corresponding logical grouping for metrics,
// and then interpolates this with the given |metric|, which should be in the
// range [0, |num_possible_metrics|).  Returns the interpolated metric.
// Clients must ensure that |field_type| is one of the types Chrome supports
// natively, e.g. |field_type| must not be a billng address.
int GetFieldTypeGroupMetric(const AutofillFieldType field_type,
                            const int metric,
                            const int num_possible_metrics) {
  DCHECK(metric < num_possible_metrics);

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

    case AutofillType::PHONE_HOME:
      group = PHONE;
      break;

    case AutofillType::PHONE_FAX:
      group = FAX;
      break;

    case AutofillType::CREDIT_CARD:
      switch (field_type) {
        case ::CREDIT_CARD_NAME:
          group = CREDIT_CARD_NAME;
          break;
        case ::CREDIT_CARD_NUMBER:
          group = CREDIT_CARD_NUMBER;
          break;
        default:
          group = CREDIT_CARD_DATE;
      }
      break;

    default:
      NOTREACHED();
      group = AMBIGUOUS;
  }

  // Interpolate the |metric| with the |group|, so that all metrics for a given
  // |group| are adjacent.  The resulting metrics will be arranged as:
  // AMBIGUOUS_UNKNOWN
  // AMBIGUOUS_MATCH
  // AMBIGUOUS_MISMATCH
  // NAME_UNKNOWN
  // NAME_MATCH
  // NAME_MISMATCH
  // ...
  return (group * num_possible_metrics) + metric;
}

// A version of the UMA_HISTOGRAM_ENUMERATION macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramEnumeration(const std::string& name,
                                int sample,
                                int boundary_value) {
  // We can't use the UMA_HISTOGRAM_ENUMERATION macro here because the histogram
  // name can vary over the duration of the program.
  // Note that this leaks memory; that is expected behavior.
  base::Histogram* counter =
      base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample);
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
  DCHECK(metric < num_possible_metrics);

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

}  // namespace

AutofillMetrics::AutofillMetrics() {
}

AutofillMetrics::~AutofillMetrics() {
}

void AutofillMetrics::Log(CreditCardInfoBarMetric metric) const {
  DCHECK(metric < NUM_CREDIT_CARD_INFO_BAR_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardInfoBar", metric,
                            NUM_CREDIT_CARD_INFO_BAR_METRICS);
}

void AutofillMetrics::Log(HeuristicTypeQualityMetric metric,
                          AutofillFieldType field_type,
                          const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.HeuristicType",
                       metric, NUM_HEURISTIC_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
}

void AutofillMetrics::Log(PredictedTypeQualityMetric metric,
                          AutofillFieldType field_type,
                          const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.PredictedType",
                       metric, NUM_PREDICTED_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
}

void AutofillMetrics::Log(QualityMetric metric,
                          const std::string& experiment_id) const {
  DCHECK(metric < NUM_QUALITY_METRICS);

  std::string histogram_name = "Autofill.Quality";
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;

  LogUMAHistogramEnumeration(histogram_name, metric, NUM_QUALITY_METRICS);
}

void AutofillMetrics::Log(ServerQueryMetric metric) const {
  DCHECK(metric < NUM_SERVER_QUERY_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

void AutofillMetrics::Log(ServerTypeQualityMetric metric,
                          AutofillFieldType field_type,
                          const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.ServerType",
                       metric, NUM_SERVER_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
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
