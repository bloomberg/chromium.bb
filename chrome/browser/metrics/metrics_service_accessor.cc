// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_service_accessor.h"

#include "chrome/browser/browser_process.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_observer.h"
#include "components/variations/metrics_util.h"

// static
void MetricsServiceAccessor::AddMetricsServiceObserver(
    MetricsServiceObserver* observer) {
  MetricsService* metrics_service = g_browser_process->metrics_service();
  if (metrics_service)
    metrics_service->AddObserver(observer);
}

void MetricsServiceAccessor::RemoveMetricsServiceObserver(
    MetricsServiceObserver* observer) {
  MetricsService* metrics_service = g_browser_process->metrics_service();
  if (metrics_service)
    metrics_service->RemoveObserver(observer);
}

// static
bool MetricsServiceAccessor::RegisterSyntheticFieldTrial(
    MetricsService* metrics_service,
    const std::string& trial,
    const std::string& group) {
  if (!metrics_service)
    return false;

  SyntheticTrialGroup trial_group(metrics::HashName(trial),
                                  metrics::HashName(group));
  metrics_service->RegisterSyntheticFieldTrial(trial_group);
  return true;
}
