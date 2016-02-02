// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"
#include "chrome/browser/metrics/variations/ui_string_overrider_factory.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Posts |GoogleUpdateSettings::StoreMetricsClientInfo| on blocking pool thread
// because it needs access to IO and cannot work from UI thread.
void PostStoreMetricsClientInfo(const metrics::ClientInfo& client_info) {
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&GoogleUpdateSettings::StoreMetricsClientInfo, client_info));
}

}  // namespace

ChromeMetricsServicesManagerClient::ChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state);

  SetupMetricsStateForChromeOS();
}

ChromeMetricsServicesManagerClient::~ChromeMetricsServicesManagerClient() {}

scoped_ptr<rappor::RapporService>
ChromeMetricsServicesManagerClient::CreateRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return make_scoped_ptr(new rappor::RapporService(
      local_state_, base::Bind(&chrome::IsOffTheRecordSessionActive)));
}

scoped_ptr<variations::VariationsService>
ChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return variations::VariationsService::Create(
      make_scoped_ptr(new ChromeVariationsServiceClient()), local_state_,
      GetMetricsStateManager(), switches::kDisableBackgroundNetworking,
      chrome_variations::CreateUIStringOverrider());
}

scoped_ptr<metrics::MetricsServiceClient>
ChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ChromeMetricsServiceClient::Create(GetMetricsStateManager(),
                                            local_state_);
}

net::URLRequestContextGetter*
ChromeMetricsServicesManagerClient::GetURLRequestContext() {
  return g_browser_process->system_request_context();
}

bool ChromeMetricsServicesManagerClient::IsSafeBrowsingEnabled(
    const base::Closure& on_update_callback) {
  // Start listening for updates to SB service state. This is done here instead
  // of in the constructor to avoid errors from trying to instantiate SB
  // service before the IO thread exists.
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_state_subscription_ && sb_service) {
    // It is safe to pass the callback received from the
    // MetricsServicesManager here since the MetricsServicesManager owns
    // this object, which owns the sb_state_subscription_, which owns the
    // pointer to the MetricsServicesManager.
    sb_state_subscription_ =
        sb_service->RegisterStateCallback(on_update_callback);
  }

  return sb_service && sb_service->enabled_by_prefs();
}

bool ChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

bool ChromeMetricsServicesManagerClient::OnlyDoMetricsRecording() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(switches::kMetricsRecordingOnly) ||
         cmdline->HasSwitch(switches::kEnableBenchmarking);
}

metrics::MetricsStateManager*
ChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_state_manager_) {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_,
        base::Bind(
            &ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled),
        base::Bind(&PostStoreMetricsClientInfo),
        base::Bind(&GoogleUpdateSettings::LoadMetricsClientInfo));
  }
  return metrics_state_manager_.get();
}
