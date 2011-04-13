// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace {

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
                          const std::string& experiment_id) const {
  DCHECK(metric < NUM_HEURISTIC_TYPE_QUALITY_METRICS);

  std::string histogram_name = "Autofill.Quality.HeuristicType";
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;

  LogUMAHistogramEnumeration(histogram_name, metric,
                             NUM_HEURISTIC_TYPE_QUALITY_METRICS);
}

void AutofillMetrics::Log(PredictedTypeQualityMetric metric,
                          const std::string& experiment_id) const {
  DCHECK(metric < NUM_PREDICTED_TYPE_QUALITY_METRICS);

  std::string histogram_name = "Autofill.Quality.PredictedType";
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;

  LogUMAHistogramEnumeration(histogram_name, metric,
                             NUM_PREDICTED_TYPE_QUALITY_METRICS);
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
                          const std::string& experiment_id) const {
  DCHECK(metric < NUM_SERVER_TYPE_QUALITY_METRICS);

  std::string histogram_name = "Autofill.Quality.ServerType";
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;

  LogUMAHistogramEnumeration(histogram_name, metric,
                             NUM_SERVER_TYPE_QUALITY_METRICS);
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
