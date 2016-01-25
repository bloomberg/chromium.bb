// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/history/metrics_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ios/public/provider/web/web_ui_ios.h"
#include "ios/web/public/user_metrics.h"

using base::ListValue;
using base::UserMetricsAction;

MetricsHandler::MetricsHandler() {}
MetricsHandler::~MetricsHandler() {}

void MetricsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordAction",
      base::Bind(&MetricsHandler::HandleRecordAction, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordInHistogram",
      base::Bind(&MetricsHandler::HandleRecordInHistogram,
                 base::Unretained(this)));
}

void MetricsHandler::HandleRecordAction(const base::ListValue* args) {
  std::string string_action = base::UTF16ToUTF8(ExtractStringValue(args));
  web::RecordComputedAction(string_action);
}

void MetricsHandler::HandleRecordInHistogram(const base::ListValue* args) {
  std::string histogram_name;
  double value;
  double boundary_value;
  if (!args->GetString(0, &histogram_name) || !args->GetDouble(1, &value) ||
      !args->GetDouble(2, &boundary_value)) {
    NOTREACHED();
    return;
  }

  int int_value = static_cast<int>(value);
  int int_boundary_value = static_cast<int>(boundary_value);
  if (int_boundary_value >= 4000 || int_value > int_boundary_value ||
      int_value < 0) {
    NOTREACHED();
    return;
  }

  int bucket_count = int_boundary_value;
  while (bucket_count >= 100) {
    bucket_count /= 10;
  }

  // As |histogram_name| may change between calls, the UMA_HISTOGRAM_ENUMERATION
  // macro cannot be used here.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram_name, 1, int_boundary_value, bucket_count + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(int_value);
}
