// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_services_manager.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/rappor/rappor_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

MetricsServicesManager::MetricsServicesManager(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state);
}

MetricsServicesManager::~MetricsServicesManager() {
}

metrics::MetricsService* MetricsServicesManager::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetChromeMetricsServiceClient()->metrics_service();
}

rappor::RapporService* MetricsServicesManager::GetRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!rappor_service_)
    rappor_service_.reset(new rappor::RapporService(local_state_));
  return rappor_service_.get();
}

chrome_variations::VariationsService*
MetricsServicesManager::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!variations_service_) {
    variations_service_ =
        chrome_variations::VariationsService::Create(local_state_,
                                                     GetMetricsStateManager());
  }
  return variations_service_.get();
}

void MetricsServicesManager::OnPluginLoadingError(
    const base::FilePath& plugin_path) {
  GetChromeMetricsServiceClient()->LogPluginLoadingError(plugin_path);
}

ChromeMetricsServiceClient*
MetricsServicesManager::GetChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_service_client_) {
    metrics_service_client_ = ChromeMetricsServiceClient::Create(
        GetMetricsStateManager(), local_state_);
  }
  return metrics_service_client_.get();
}

metrics::MetricsStateManager* MetricsServicesManager::GetMetricsStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_state_manager_) {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_,
        base::Bind(&MetricsServicesManager::IsMetricsReportingEnabled,
                   base::Unretained(this)),
        base::Bind(&GoogleUpdateSettings::StoreMetricsClientInfo),
        base::Bind(&GoogleUpdateSettings::LoadMetricsClientInfo));
  }
  return metrics_state_manager_.get();
}

// TODO(asvitkine): This function does not report the correct value on Android,
// see http://crbug.com/362192.
bool MetricsServicesManager::IsMetricsReportingEnabled() const {
  // If the user permits metrics reporting with the checkbox in the
  // prefs, we turn on recording.  We disable metrics completely for
  // non-official builds, or when field trials are forced.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kForceFieldTrials))
    return false;

  bool enabled = false;
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &enabled);
#else
  enabled = local_state_->GetBoolean(prefs::kMetricsReportingEnabled);
#endif  // #if defined(OS_CHROMEOS)
#endif  // defined(GOOGLE_CHROME_BUILD)
  return enabled;
}
