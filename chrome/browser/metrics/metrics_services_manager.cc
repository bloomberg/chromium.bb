// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_services_manager.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"
#include "chrome/browser/metrics/variations/ui_string_overrider_factory.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/rappor/rappor_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

// Posts |GoogleUpdateSettings::StoreMetricsClientInfo| on blocking pool thread
// because it needs access to IO and cannot work from UI thread.
void PostStoreMetricsClientInfo(const metrics::ClientInfo& client_info) {
  content::BrowserThread::GetBlockingPool()->PostTask(FROM_HERE,
      base::Bind(&GoogleUpdateSettings::StoreMetricsClientInfo, client_info));
}

MetricsServicesManager::MetricsServicesManager(PrefService* local_state)
    : local_state_(local_state),
      may_upload_(false),
      may_record_(false) {
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
  if (!rappor_service_) {
    rappor_service_.reset(new rappor::RapporService(
        local_state_, base::Bind(&chrome::IsOffTheRecordSessionActive)));
    rappor_service_->Initialize(g_browser_process->system_request_context());
  }
  return rappor_service_.get();
}

variations::VariationsService* MetricsServicesManager::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!variations_service_) {
    variations_service_ = variations::VariationsService::Create(
        make_scoped_ptr(new ChromeVariationsServiceClient()), local_state_,
        GetMetricsStateManager(), switches::kDisableBackgroundNetworking,
        chrome_variations::CreateUIStringOverrider());
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
        base::Bind(
            &ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled),
        base::Bind(&PostStoreMetricsClientInfo),
        base::Bind(&GoogleUpdateSettings::LoadMetricsClientInfo));
  }
  return metrics_state_manager_.get();
}

bool MetricsServicesManager::GetSafeBrowsingState() {
  // Start listening for updates to SB service state. This is done here instead
  // of in the constructor to avoid errors from trying to instantiate SB
  // service before the IO thread exists.
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  if (!sb_state_subscription_ && sb_service) {
    // base::Unretained(this) is safe here since this object owns the
    // sb_state_subscription_ which owns the pointer.
    sb_state_subscription_ = sb_service->RegisterStateCallback(
        base::Bind(&MetricsServicesManager::UpdateRunningServices,
                   base::Unretained(this)));
  }

  return sb_service && sb_service->enabled_by_prefs();
}

void MetricsServicesManager::UpdatePermissions(bool may_record,
                                               bool may_upload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Stash the current permissions so that we can update the RapporService
  // correctly when the Rappor preference changes.  The metrics recording
  // preference partially determines the initial rappor setting, and also
  // controls whether FINE metrics are sent.
  may_record_ = may_record;
  may_upload_ = may_upload;
  UpdateRunningServices();
}

void MetricsServicesManager::UpdateRunningServices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics::MetricsService* metrics = GetMetricsService();

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  const bool only_do_metrics_recording =
      cmdline->HasSwitch(switches::kMetricsRecordingOnly) ||
      cmdline->HasSwitch(switches::kEnableBenchmarking);

  if (only_do_metrics_recording) {
    metrics->StartRecordingForTests();
    GetRapporService()->Update(
        rappor::UMA_RAPPOR_GROUP | rappor::SAFEBROWSING_RAPPOR_GROUP,
        false);
    return;
  }

  if (may_record_) {
    if (!metrics->recording_active())
      metrics->Start();

    if (may_upload_)
      metrics->EnableReporting();
    else
      metrics->DisableReporting();
  } else {
    metrics->Stop();
  }

  int recording_groups = 0;
#if defined(GOOGLE_CHROME_BUILD)
  if (may_record_)
    recording_groups |= rappor::UMA_RAPPOR_GROUP;
  if (GetSafeBrowsingState())
    recording_groups |= rappor::SAFEBROWSING_RAPPOR_GROUP;
#endif  // defined(GOOGLE_CHROME_BUILD)
  GetRapporService()->Update(recording_groups, may_upload_);
}

void MetricsServicesManager::UpdateUploadPermissions(bool may_upload) {
  return UpdatePermissions(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled(),
      may_upload);
}
