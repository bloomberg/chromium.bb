// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
#define CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_

#include "base/callback.h"

typedef base::Callback<void(bool)> OnMetricsReportingCallbackType;

// Initiates a change to metrics reporting state to the new value of |enabled|.
// Starts or stops the metrics service based on the new state and then runs
// |callback_fn| (which can be null) with the updated state (as the operation
// may fail). On platforms other than CrOS and Android, also updates the
// underlying pref.
// TODO(gayane): Support setting the pref on all platforms.
void InitiateMetricsReportingChange(
    bool enabled,
    const OnMetricsReportingCallbackType& callback_fn);

// Returns whether MetricsReporting can be modified by the user (except CrOS and
// Android).
bool IsMetricsReportingUserChangable();

#endif  // CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
