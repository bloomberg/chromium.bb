// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

StabilityMetricsProvider::StabilityMetricsProvider(PrefService* local_state)
    : local_state_(local_state) {}

StabilityMetricsProvider::~StabilityMetricsProvider() = default;

// static
void StabilityMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityIncompleteSessionEndCount, 0);
  registry->RegisterBooleanPref(prefs::kStabilitySessionEndCompleted, true);
  registry->RegisterIntegerPref(prefs::kStabilityLaunchCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityBreakpadRegistrationFail, 0);
  registry->RegisterIntegerPref(prefs::kStabilityBreakpadRegistrationSuccess,
                                0);
  registry->RegisterIntegerPref(prefs::kStabilityDebuggerPresent, 0);
  registry->RegisterIntegerPref(prefs::kStabilityDebuggerNotPresent, 0);
  registry->RegisterIntegerPref(prefs::kStabilityDeferredCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityDiscardCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityVersionMismatchCount, 0);
}

void StabilityMetricsProvider::ClearSavedStabilityMetrics() {
  local_state_->SetInteger(prefs::kStabilityCrashCount, 0);
  local_state_->SetInteger(prefs::kStabilityIncompleteSessionEndCount, 0);
  local_state_->SetInteger(prefs::kStabilityBreakpadRegistrationSuccess, 0);
  local_state_->SetInteger(prefs::kStabilityBreakpadRegistrationFail, 0);
  local_state_->SetInteger(prefs::kStabilityDebuggerPresent, 0);
  local_state_->SetInteger(prefs::kStabilityDebuggerNotPresent, 0);
  local_state_->SetInteger(prefs::kStabilityLaunchCount, 0);
  local_state_->SetBoolean(prefs::kStabilitySessionEndCompleted, true);
  local_state_->SetInteger(prefs::kStabilityDeferredCount, 0);
  // Note: kStabilityDiscardCount is not cleared as its intent is to measure
  // the number of times data is discarded, even across versions.
  local_state_->SetInteger(prefs::kStabilityVersionMismatchCount, 0);
}

void StabilityMetricsProvider::ProvideStabilityMetrics(
    SystemProfileProto* system_profile) {
  SystemProfileProto::Stability* stability =
      system_profile->mutable_stability();

  int launch_count = local_state_->GetInteger(prefs::kStabilityLaunchCount);
  if (launch_count) {
    local_state_->SetInteger(prefs::kStabilityLaunchCount, 0);
    stability->set_launch_count(launch_count);
  }
  int crash_count = local_state_->GetInteger(prefs::kStabilityCrashCount);
  if (crash_count) {
    local_state_->SetInteger(prefs::kStabilityCrashCount, 0);
    stability->set_crash_count(crash_count);
  }

  int incomplete_shutdown_count =
      local_state_->GetInteger(prefs::kStabilityIncompleteSessionEndCount);
  if (incomplete_shutdown_count) {
    local_state_->SetInteger(prefs::kStabilityIncompleteSessionEndCount, 0);
    stability->set_incomplete_shutdown_count(incomplete_shutdown_count);
  }

  int breakpad_registration_success_count =
      local_state_->GetInteger(prefs::kStabilityBreakpadRegistrationSuccess);
  if (breakpad_registration_success_count) {
    local_state_->SetInteger(prefs::kStabilityBreakpadRegistrationSuccess, 0);
    stability->set_breakpad_registration_success_count(
        breakpad_registration_success_count);
  }

  int breakpad_registration_failure_count =
      local_state_->GetInteger(prefs::kStabilityBreakpadRegistrationFail);
  if (breakpad_registration_failure_count) {
    local_state_->SetInteger(prefs::kStabilityBreakpadRegistrationFail, 0);
    stability->set_breakpad_registration_failure_count(
        breakpad_registration_failure_count);
  }

  int debugger_present_count =
      local_state_->GetInteger(prefs::kStabilityDebuggerPresent);
  if (debugger_present_count) {
    local_state_->SetInteger(prefs::kStabilityDebuggerPresent, 0);
    stability->set_debugger_present_count(debugger_present_count);
  }

  int debugger_not_present_count =
      local_state_->GetInteger(prefs::kStabilityDebuggerNotPresent);
  if (debugger_not_present_count) {
    local_state_->SetInteger(prefs::kStabilityDebuggerNotPresent, 0);
    stability->set_debugger_not_present_count(debugger_not_present_count);
  }

  // Note: only logging the following histograms for non-zero values.
  int deferred_count = local_state_->GetInteger(prefs::kStabilityDeferredCount);
  if (deferred_count) {
    local_state_->SetInteger(prefs::kStabilityDeferredCount, 0);
    UMA_STABILITY_HISTOGRAM_COUNTS_100(
        "Stability.Internals.InitialStabilityLogDeferredCount", deferred_count);
  }

  int discard_count = local_state_->GetInteger(prefs::kStabilityDiscardCount);
  if (discard_count) {
    local_state_->SetInteger(prefs::kStabilityDiscardCount, 0);
    UMA_STABILITY_HISTOGRAM_COUNTS_100("Stability.Internals.DataDiscardCount",
                                       discard_count);
  }

  int version_mismatch_count =
      local_state_->GetInteger(prefs::kStabilityVersionMismatchCount);
  if (version_mismatch_count) {
    local_state_->SetInteger(prefs::kStabilityVersionMismatchCount, 0);
    UMA_STABILITY_HISTOGRAM_COUNTS_100(
        "Stability.Internals.VersionMismatchCount", version_mismatch_count);
  }
}

void StabilityMetricsProvider::RecordBreakpadRegistration(bool success) {
  if (!success)
    IncrementPrefValue(prefs::kStabilityBreakpadRegistrationFail);
  else
    IncrementPrefValue(prefs::kStabilityBreakpadRegistrationSuccess);
}

void StabilityMetricsProvider::RecordBreakpadHasDebugger(bool has_debugger) {
  if (!has_debugger)
    IncrementPrefValue(prefs::kStabilityDebuggerNotPresent);
  else
    IncrementPrefValue(prefs::kStabilityDebuggerPresent);
}

void StabilityMetricsProvider::CheckLastSessionEndCompleted() {
  if (!local_state_->GetBoolean(prefs::kStabilitySessionEndCompleted)) {
    IncrementPrefValue(prefs::kStabilityIncompleteSessionEndCount);
    // This is marked false when we get a WM_ENDSESSION.
    MarkSessionEndCompleted(true);
  }
}

void StabilityMetricsProvider::MarkSessionEndCompleted(bool end_completed) {
  local_state_->SetBoolean(prefs::kStabilitySessionEndCompleted, end_completed);
}

void StabilityMetricsProvider::LogCrash() {
  IncrementPrefValue(prefs::kStabilityCrashCount);
}

void StabilityMetricsProvider::LogStabilityLogDeferred() {
  IncrementPrefValue(prefs::kStabilityDeferredCount);
}

void StabilityMetricsProvider::LogStabilityDataDiscarded() {
  IncrementPrefValue(prefs::kStabilityDiscardCount);
}

void StabilityMetricsProvider::LogLaunch() {
  IncrementPrefValue(prefs::kStabilityLaunchCount);
}

void StabilityMetricsProvider::LogStabilityVersionMismatch() {
  IncrementPrefValue(prefs::kStabilityVersionMismatchCount);
}

void StabilityMetricsProvider::IncrementPrefValue(const char* path) {
  int value = local_state_->GetInteger(path);
  local_state_->SetInteger(path, value + 1);
}

}  // namespace metrics
