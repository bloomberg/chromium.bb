// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager_metrics.h"

#include "base/metrics/histogram.h"

namespace {

const char kTranslateInitiationStatus[] = "Translate.InitiationStatus";

}  // namespace

namespace TranslateManagerMetrics {

void ReportInitiationStatus(InitiationStatusType type) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateInitiationStatus,
                            type,
                            INITIATION_STATUS_MAX);
}

const char* GetMetricsName(MetricsNameIndex index) {
  const char* names[] = {
    kTranslateInitiationStatus,
  };
  CHECK(index < UMA_MAX);
  return names[index];
}

// TODO(toyoshim): Move UMA related code in TranslateManager to
// TranslateManagerMetrics.

}  // namespace TranslateManagerMetrics
