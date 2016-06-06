// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_default_state.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

void RegisterMetricsReportingStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kMetricsDefaultOptIn,
                                MetricsServiceClient::DEFAULT_UNKNOWN);
}

void RecordMetricsReportingDefaultOptIn(PrefService* local_state, bool opt_in) {
  DCHECK(GetMetricsReportingDefaultOptIn(local_state) ==
         MetricsServiceClient::DEFAULT_UNKNOWN);
  local_state->SetInteger(
      prefs::kMetricsDefaultOptIn,
      opt_in ? MetricsServiceClient::OPT_IN : MetricsServiceClient::OPT_OUT);
}

MetricsServiceClient::EnableMetricsDefault GetMetricsReportingDefaultOptIn(
    PrefService* local_state) {
  return static_cast<MetricsServiceClient::EnableMetricsDefault>(
      local_state->GetInteger(prefs::kMetricsDefaultOptIn));
}

}  // namespace metrics