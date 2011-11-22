// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_metrics_module.h"

#include "base/metrics/histogram.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/user_metrics.h"

using base::Histogram;
using base::LinearHistogram;

bool MetricsRecordUserActionFunction::RunImpl() {
  std::string name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &name));

  UserMetrics::RecordComputedAction(name);
  return true;
}

bool MetricsHistogramHelperFunction::GetNameAndSample(std::string* name,
                                                      int* sample) {
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, sample));
  return true;
}

bool MetricsHistogramHelperFunction::RecordValue(const std::string& name,
                                                 Histogram::ClassType type,
                                                 int min,
                                                 int max,
                                                 size_t buckets,
                                                 int sample) {
  Histogram* counter;
  if (type == Histogram::LINEAR_HISTOGRAM) {
    counter = LinearHistogram::FactoryGet(name,
                                          min,
                                          max,
                                          buckets,
                                          Histogram::kUmaTargetedHistogramFlag);
  } else {
    counter = Histogram::FactoryGet(name,
                                    min,
                                    max,
                                    buckets,
                                    Histogram::kUmaTargetedHistogramFlag);
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

  Histogram::ClassType histogram_type(type == "histogram-linear" ?
      Histogram::LINEAR_HISTOGRAM : Histogram::HISTOGRAM);
  return RecordValue(name, histogram_type, min, max, buckets, sample);
}

bool MetricsRecordPercentageFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  return RecordValue(name, Histogram::LINEAR_HISTOGRAM, 1, 101, 102, sample);
}

bool MetricsRecordCountFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  return RecordValue(name, Histogram::HISTOGRAM, 1, 1000000, 50, sample);
}

bool MetricsRecordSmallCountFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  return RecordValue(name, Histogram::HISTOGRAM, 1, 100, 50, sample);
}

bool MetricsRecordMediumCountFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  return RecordValue(name, Histogram::HISTOGRAM, 1, 10000, 50, sample);
}

bool MetricsRecordTimeFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  static const int kTenSecMs = 10 * 1000;
  return RecordValue(name, Histogram::HISTOGRAM, 1, kTenSecMs, 50, sample);
}

bool MetricsRecordMediumTimeFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  static const int kThreeMinMs = 3 * 60 * 1000;
  return RecordValue(name, Histogram::HISTOGRAM, 1, kThreeMinMs, 50, sample);
}

bool MetricsRecordLongTimeFunction::RunImpl() {
  std::string name;
  int sample;
  EXTENSION_FUNCTION_VALIDATE(GetNameAndSample(&name, &sample));
  static const int kOneHourMs = 60 * 60 * 1000;
  return RecordValue(name, Histogram::HISTOGRAM, 1, kOneHourMs, 50, sample);
}
