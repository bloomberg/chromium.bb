// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_pref_names.h"

namespace metrics {
namespace prefs {

// Set once, to the current epoch time, on the first run of chrome on this
// machine. Attached to metrics reports forever thereafter.
const char kInstallDate[] = "uninstall_metrics.installation_date2";

// The metrics client GUID.
// Note: The name client_id2 is a result of creating
// new prefs to do a one-time reset of the previous values.
const char kMetricsClientID[] = "user_experience_metrics.client_id2";

// Array of strings that are each UMA logs that were supposed to be sent in the
// first minute of a browser session. These logs include things like crash count
// info, etc.
const char kMetricsInitialLogs[] =
    "user_experience_metrics.initial_logs_list";

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
    "user_experience_metrics.ongoing_logs_list";

// Boolean that indicates a cloned install has been detected and the metrics
// client id and low entropy source should be reset.
const char kMetricsResetIds[] = "user_experience_metrics.reset_metrics_ids";

// Date/time when the user opted in to UMA and generated the client id for the
// very first time (local machine time, stored as a 64-bit time_t value).
const char kMetricsReportingEnabledTimestamp[] =
    "user_experience_metrics.client_id_timestamp";

// The metrics client session ID.
const char kMetricsSessionID[] = "user_experience_metrics.session_id";

// Number of times the browser has been able to register crash reporting.
const char kStabilityBreakpadRegistrationSuccess[] =
    "user_experience_metrics.stability.breakpad_registration_ok";

// Number of times the browser has failed to register crash reporting.
const char kStabilityBreakpadRegistrationFail[] =
    "user_experience_metrics.stability.breakpad_registration_fail";

// Number of times the application exited uncleanly since the last report.
const char kStabilityCrashCount[] =
    "user_experience_metrics.stability.crash_count";

// Number of times the browser has been run under a debugger.
const char kStabilityDebuggerPresent[] =
    "user_experience_metrics.stability.debugger_present";

// Number of times the browser has not been run under a debugger.
const char kStabilityDebuggerNotPresent[] =
    "user_experience_metrics.stability.debugger_not_present";

// An enum value to indicate the execution phase the browser was in.
const char kStabilityExecutionPhase[] =
    "user_experience_metrics.stability.execution_phase";

// True if the previous run of the program exited cleanly.
const char kStabilityExitedCleanly[] =
    "user_experience_metrics.stability.exited_cleanly";

// Number of times the session end did not complete.
const char kStabilityIncompleteSessionEndCount[] =
    "user_experience_metrics.stability.incomplete_session_end_count";

// Time when the app was last known to be running, in seconds since
// the epoch.
const char kStabilityLastTimestampSec[] =
    "user_experience_metrics.stability.last_timestamp_sec";

// Number of times the application was launched since last report.
const char kStabilityLaunchCount[] =
    "user_experience_metrics.stability.launch_count";

// Time when the app was last launched, in seconds since the epoch.
const char kStabilityLaunchTimeSec[] =
    "user_experience_metrics.stability.launch_time_sec";

// Base64 encoded serialized UMA system profile proto from the previous session.
const char kStabilitySavedSystemProfile[] =
    "user_experience_metrics.stability.saved_system_profile";

// SHA-1 hash of the serialized UMA system profile proto (hex encoded).
const char kStabilitySavedSystemProfileHash[] =
    "user_experience_metrics.stability.saved_system_profile_hash";

// False if we received a session end and either we crashed during processing
// the session end or ran out of time and windows terminated us.
const char kStabilitySessionEndCompleted[] =
    "user_experience_metrics.stability.session_end_completed";

// Build time, in seconds since an epoch, which is used to assure that stability
// metrics reported reflect stability of the same build.
const char kStabilityStatsBuildTime[] =
    "user_experience_metrics.stability.stats_buildtime";

// Version string of previous run, which is used to assure that stability
// metrics reported under current version reflect stability of the same version.
const char kStabilityStatsVersion[] =
    "user_experience_metrics.stability.stats_version";

// The keys below are strictly increasing counters over the lifetime of
// a chrome installation. They are (optionally) sent up to the uninstall
// survey in the event of uninstallation.
const char kUninstallLaunchCount[] = "uninstall_metrics.launch_count";
const char kUninstallMetricsUptimeSec[] = "uninstall_metrics.uptime_sec";

}  // namespace prefs
}  // namespace metrics
