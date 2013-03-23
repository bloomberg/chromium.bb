// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/metrics_private/metrics_private_api.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "chrome/common/extensions/api/metrics_private.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/user_metrics.h"

namespace extensions {

namespace RecordUserAction = api::metrics_private::RecordUserAction;
namespace RecordValue = api::metrics_private::RecordValue;
namespace RecordPercentage = api::metrics_private::RecordPercentage;
namespace RecordCount = api::metrics_private::RecordCount;
namespace RecordSmallCount = api::metrics_private::RecordSmallCount;
namespace RecordMediumCount = api::metrics_private::RecordMediumCount;
namespace RecordTime = api::metrics_private::RecordTime;
namespace RecordMediumTime = api::metrics_private::RecordMediumTime;
namespace RecordLongTime = api::metrics_private::RecordLongTime;

namespace {

const size_t kMaxBuckets = 10000; // We don't ever want more than these many
                                  // buckets; there is no real need for them
                                  // and would cause crazy memory usage
} // namespace

bool MetricsPrivateRecordUserActionFunction::RunImpl() {
  scoped_ptr<RecordUserAction::Params> params(
      RecordUserAction::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RecordComputedAction(params->name);
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

bool MetricsPrivateRecordValueFunction::RunImpl() {
  scoped_ptr<RecordValue::Params> params(RecordValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Get the histogram parameters from the metric type object.
  std::string type = api::metrics_private::MetricType::ToString(
      params->metric.type);

  base::HistogramType histogram_type(type == "histogram-linear" ?
      base::LINEAR_HISTOGRAM : base::HISTOGRAM);
  return RecordValue(params->metric.metric_name, histogram_type,
                     params->metric.min, params->metric.max,
                     params->metric.buckets, params->value);
}

bool MetricsPrivateRecordPercentageFunction::RunImpl() {
  scoped_ptr<RecordPercentage::Params> params(
      RecordPercentage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  return RecordValue(params->metric_name, base::LINEAR_HISTOGRAM,
                     1, 101, 102, params->value);
}

bool MetricsPrivateRecordCountFunction::RunImpl() {
  scoped_ptr<RecordCount::Params> params(RecordCount::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  return RecordValue(params->metric_name, base::HISTOGRAM,
                     1, 1000000, 50, params->value);
}

bool MetricsPrivateRecordSmallCountFunction::RunImpl() {
  scoped_ptr<RecordSmallCount::Params> params(
      RecordSmallCount::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  return RecordValue(params->metric_name, base::HISTOGRAM,
                     1, 100, 50, params->value);
}

bool MetricsPrivateRecordMediumCountFunction::RunImpl() {
  scoped_ptr<RecordMediumCount::Params> params(
      RecordMediumCount::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  return RecordValue(params->metric_name, base::HISTOGRAM,
                     1, 10000, 50, params->value);
}

bool MetricsPrivateRecordTimeFunction::RunImpl() {
  scoped_ptr<RecordTime::Params> params(RecordTime::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  static const int kTenSecMs = 10 * 1000;
  return RecordValue(params->metric_name, base::HISTOGRAM,
                     1, kTenSecMs, 50, params->value);
}

bool MetricsPrivateRecordMediumTimeFunction::RunImpl() {
  scoped_ptr<RecordMediumTime::Params> params(
      RecordMediumTime::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  static const int kThreeMinMs = 3 * 60 * 1000;
  return RecordValue(params->metric_name, base::HISTOGRAM,
                     1, kThreeMinMs, 50, params->value);
}

bool MetricsPrivateRecordLongTimeFunction::RunImpl() {
  scoped_ptr<RecordLongTime::Params> params(
      RecordLongTime::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  static const int kOneHourMs = 60 * 60 * 1000;
  return RecordValue(params->metric_name, base::HISTOGRAM,
                     1, kOneHourMs, 50, params->value);
}

} // namespace extensions
