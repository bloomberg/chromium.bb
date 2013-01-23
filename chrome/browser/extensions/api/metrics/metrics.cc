// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/metrics/metrics.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/user_metrics.h"

namespace extensions {

namespace {

const size_t kMaxBuckets = 10000; // We don't ever want more than these many
                                  // buckets; there is no real need for them
                                  // and would cause crazy memory usage
} // namespace

bool MetricsRecordUserActionFunction::RunImpl() {
  std::string name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &name));

  content::RecordComputedAction(name);
  return true;
}

bool MetricsHistogramHelperFunction::GetNameAndSample(std::string* name,
                                                      int* sample) {
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, sample));
  return true;
}

bool MetricsHistogramHelperFunction::RecordValue(
    const std::string& name,
    base::HistogramType type,
    int min, int max, size_t buckets,
    int sample) {
  // Make sure toxic values don't get to internal code.
  // Fix for maximums
  min = std::min(min, INT_MAX - 3);
  max = std::min(max, INT_MAX - 3);
  buckets = std::min(buckets, kMaxBuckets);
  // Fix for minimums.
  min = std::max(min, 1);
  max = std::max(max, min + 1);
  buckets = std::max(buckets, static_cast<size_t>(3));
  // Trim buckets down to a maximum of the given range + over/underflow buckets
  if (buckets > static_cast<size_t>(max - min + 2))
    buckets = max - min + 2;

  base::HistogramBase* counter;
  if (type == base::LINEAR_HISTOGRAM) {
    counter = base::LinearHistogram::FactoryGet(
        name, min, max, buckets,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  } else {
    counter = base::Histogram::FactoryGet(
        name, min, max, buckets,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  }

  counter->Add(sample);
  return true;
}

bool MetricsRecordValueFunction::RunImpl() {
  int sample;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &sample));

  // Get the histogram parameters from the metric type object.
  DictionaryValue* metric_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &metric_type));

  std::string name;
  std::string type;
  int min;
  int max;
  int buckets;
  EXTENSION_FUNCTION_VALIDATE(metric_type->GetString("metricName", &name));
  EXTENSION_FUNCTION_VALIDATE(metric_type->GetString("type", &type));
  EXTENSION_FUNCTION_VALIDATE(metric_type->GetInteger("min", &min));
  EXTENSION_FUNCTION_VALIDATE(metric_type->GetInteger("max", &max));
  EXTENSION_FUNCTION_VALIDATE(metric_type->GetInteger("buckets", &buckets));

  base::HistogramType histogram_type(type == "histogram-linear" ?
      base::LINEAR_HISTOGRAM : base::HISTOGRAM);
  return RecordValue(name, histogram_type, min, max, buckets, sample);
}

bool MetricsRecordPercentageFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  return RecordValue(name, base::LINEAR_HISTOGRAM, 1, 101, 102, sample);
}

bool MetricsRecordCountFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  return RecordValue(name, base::HISTOGRAM, 1, 1000000, 50, sample);
}

bool MetricsRecordSmallCountFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  return RecordValue(name, base::HISTOGRAM, 1, 100, 50, sample);
}

bool MetricsRecordMediumCountFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  return RecordValue(name, base::HISTOGRAM, 1, 10000, 50, sample);
}

bool MetricsRecordTimeFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  static const int kTenSecMs = 10 * 1000;
  return RecordValue(name, base::HISTOGRAM, 1, kTenSecMs, 50, sample);
}

bool MetricsRecordMediumTimeFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  static const int kThreeMinMs = 3 * 60 * 1000;
  return RecordValue(name, base::HISTOGRAM, 1, kThreeMinMs, 50, sample);
}

bool MetricsRecordLongTimeFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  static const int kOneHourMs = 60 * 60 * 1000;
  return RecordValue(name, base::HISTOGRAM, 1, kOneHourMs, 50, sample);
}

} // namespace extensions
