// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

namespace arc {

ArcOptInPreferenceHandler::ArcOptInPreferenceHandler(
    ArcOptInPreferenceHandlerObserver* observer,
    PrefService* pref_service)
    : observer_(observer), pref_service_(pref_service) {
  DCHECK(observer_);
  DCHECK(pref_service_);
}

void ArcOptInPreferenceHandler::Start() {
  if (g_browser_process->local_state()) {
    pref_local_change_registrar_.Init(g_browser_process->local_state());
    pref_local_change_registrar_.Add(
        metrics::prefs::kMetricsReportingEnabled,
        base::Bind(&ArcOptInPreferenceHandler::OnMetricsPreferenceChanged,
                   base::Unretained(this)));
  }

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kArcBackupRestoreEnabled,
      base::Bind(
          &ArcOptInPreferenceHandler::OnBackupAndRestorePreferenceChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kArcLocationServiceEnabled,
      base::Bind(&ArcOptInPreferenceHandler::OnLocationServicePreferenceChanged,
                 base::Unretained(this)));

  // Send current state.
  SendMetricsMode();
  SendBackupAndRestoreMode();
  SendLocationServicesMode();
}

ArcOptInPreferenceHandler::~ArcOptInPreferenceHandler() {}

void ArcOptInPreferenceHandler::OnMetricsPreferenceChanged() {
  SendMetricsMode();
}

void ArcOptInPreferenceHandler::OnBackupAndRestorePreferenceChanged() {
  SendBackupAndRestoreMode();
}

void ArcOptInPreferenceHandler::OnLocationServicePreferenceChanged() {
  SendLocationServicesMode();
}

void ArcOptInPreferenceHandler::SendMetricsMode() {
  if (g_browser_process->local_state()) {
    observer_->OnMetricsModeChanged(
        ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled(),
        IsMetricsReportingPolicyManaged());
  }
}

void ArcOptInPreferenceHandler::SendBackupAndRestoreMode() {
  observer_->OnBackupAndRestoreModeChanged(
      pref_service_->GetBoolean(prefs::kArcBackupRestoreEnabled),
      pref_service_->IsManagedPreference(prefs::kArcBackupRestoreEnabled));
}

void ArcOptInPreferenceHandler::SendLocationServicesMode() {
  observer_->OnLocationServicesModeChanged(
      pref_service_->GetBoolean(prefs::kArcLocationServiceEnabled),
      pref_service_->IsManagedPreference(prefs::kArcLocationServiceEnabled));
}

void ArcOptInPreferenceHandler::EnableMetrics(bool is_enabled) {
  if (g_browser_process->local_state())
    ChangeMetricsReportingState(is_enabled);
}

void ArcOptInPreferenceHandler::EnableBackupRestore(bool is_enabled) {
  pref_service_->SetBoolean(prefs::kArcBackupRestoreEnabled, is_enabled);
}

void ArcOptInPreferenceHandler::EnableLocationService(bool is_enabled) {
  pref_service_->SetBoolean(prefs::kArcLocationServiceEnabled, is_enabled);
}

}  // namespace arc
