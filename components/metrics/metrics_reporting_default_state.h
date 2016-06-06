// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_REPORTING_DEFAULT_STATE_H_
#define COMPONENTS_METRICS_METRICS_REPORTING_DEFAULT_STATE_H_

#include "components/metrics/metrics_service_client.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// Register prefs relating to metrics reporting state. Currently only registers
// a pref for metrics reporting default opt-in state.
void RegisterMetricsReportingStatePrefs(PrefRegistrySimple* registry);

// Sets whether metrics reporting was opt-in or not. If it was opt-in, then the
// enable checkbox on first-run was default unchecked. If it was opt-out, then
// the checkbox was default checked. This should only be set once, and only
// during first-run.
void RecordMetricsReportingDefaultOptIn(PrefService* local_state, bool opt_in);

// Gets information about the default value for the enable metrics reporting
// checkbox shown during first-run.
MetricsServiceClient::EnableMetricsDefault GetMetricsReportingDefaultOptIn(
    PrefService* local_state);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_REPORTING_DEFAULT_STATE_H_