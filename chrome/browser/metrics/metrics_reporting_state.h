// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
#define CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_

#include "base/callback.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

typedef base::Callback<void(bool)> OnMetricsReportingCallbackType;

namespace metrics {
class MetricsService;
}

namespace metrics_services_manager {
class MetricsServicesManager;
}

// Many of these functions take objects available from |g_browser_process|. For
// example, ChangeMetricsReportingState() takes local state. This is done as
// during startup some of this code is run at a point when |g_browser_process|
// may not have been created. Unless your code is run during early
// initialization use |g_browser_process| to obtain the appropriate value.

// Changes metrics reporting state without caring about the success of the
// change. See early comment for details on args.
void ChangeMetricsReportingState(
    PrefService* local_state,
    metrics_services_manager::MetricsServicesManager* metrics_services_manager,
    bool enabled);

// Changes metrics reporting state to the new value of |enabled|. Starts or
// stops the metrics service based on the new state and then runs |callback_fn|
// (which can be null) with the updated state (as the operation may fail). On
// platforms other than CrOS and Android, also updates the underlying pref.
// See early comment for details on args.
// TODO(gayane): Support setting the pref on all platforms.
void ChangeMetricsReportingStateWithReply(
    PrefService* local_state,
    metrics_services_manager::MetricsServicesManager* metrics_services_manager,
    bool enabled,
    const OnMetricsReportingCallbackType& callback_fn);

// Update metrics prefs on a permission (opt-in/out) change. When opting out,
// this clears various client ids. When opting in, this resets saving crash
// prefs, so as not to trigger upload of various stale data.
// See early comment for details on args.
void UpdateMetricsPrefsOnPermissionChange(
    PrefService* local_state,
    metrics::MetricsService* metrics_service,
    bool metrics_enabled);

// Returns whether MetricsReporting can be modified by the user (except
// Android).
// See early comment for details on args.
bool IsMetricsReportingPolicyManaged(PrefService* local_state);

#endif  // CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
