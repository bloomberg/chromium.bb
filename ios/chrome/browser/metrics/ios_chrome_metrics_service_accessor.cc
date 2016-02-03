// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"

#include <stdint.h>

#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"

// static
bool IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() {
  return IsMetricsReportingEnabled(GetApplicationContext()->GetLocalState());
}

// static
bool IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
    const std::string& trial_name,
    const std::string& group_name) {
  return metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial(
      GetApplicationContext()->GetMetricsService(), trial_name, group_name);
}

// static
bool IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash(
    uint32_t trial_name_hash,
    const std::string& group_name) {
  return metrics::MetricsServiceAccessor::
      RegisterSyntheticFieldTrialWithNameHash(
          GetApplicationContext()->GetMetricsService(), trial_name_hash,
          group_name);
}
