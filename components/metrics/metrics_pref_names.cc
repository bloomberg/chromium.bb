// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_pref_names.h"

namespace metrics {
namespace prefs {

// The metrics client GUID.
// Note: The name client_id2 is a result of creating
// new prefs to do a one-time reset of the previous values.
const char kMetricsClientID[] = "user_experience_metrics.client_id2";

// Array of strings that are each UMA logs that were supposed to be sent in the
// first minute of a browser session. These logs include things like crash count
// info, etc.
const char kMetricsInitialLogs[] =
    "user_experience_metrics.initial_logs_as_protobufs";

// The metrics entropy source.
// Note: The name low_entropy_source2 is a result of creating
// new prefs to do a one-time reset of the previous values.
const char kMetricsLowEntropySource[] =
    "user_experience_metrics.low_entropy_source2";

// A machine ID used to detect when underlying hardware changes. It is only
// stored locally and never transmitted in metrics reports.
const char kMetricsMachineId[] = "user_experience_metrics.machine_id";

// Old client id and low entropy source values, cleared the first time this
// version is launched.
// TODO(asvitkine): Delete these after a few releases have gone by and old
// values have been cleaned up. http://crbug.com/357704
const char kMetricsOldClientID[] = "user_experience_metrics.client_id";
const char kMetricsOldLowEntropySource[] =
    "user_experience_metrics.low_entropy_source";

// Array of strings that are each UMA logs that were not sent because the
// browser terminated before these accumulated metrics could be sent.  These
// logs typically include histograms and memory reports, as well as ongoing
// user activities.
const char kMetricsOngoingLogs[] =
    "user_experience_metrics.ongoing_logs_as_protobufs";

// Boolean that indicates a cloned install has been detected and the metrics
// client id and low entropy source should be reset.
const char kMetricsResetIds[] = "user_experience_metrics.reset_metrics_ids";

// Date/time when the user opted in to UMA and generated the client id for the
// very first time (local machine time, stored as a 64-bit time_t value).
const char kMetricsReportingEnabledTimestamp[] =
    "user_experience_metrics.client_id_timestamp";

// The metrics client session ID.
const char kMetricsSessionID[] = "user_experience_metrics.session_id";

}  // namespace prefs
}  // namespace metrics
