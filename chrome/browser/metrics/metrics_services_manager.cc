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
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/rappor/rappor_service.h"
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
  pref_change_registrar_.Init(local_state);
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
        base::Bind(&ChromeMetricsServiceAccessor::IsMetricsReportingEnabled),
        base::Bind(&PostStoreMetricsClientInfo),
        base::Bind(&GoogleUpdateSettings::LoadMetricsClientInfo));
  }
  return metrics_state_manager_.get();
}

void MetricsServicesManager::UpdatePermissions(bool may_record,
                                               bool may_upload) {
  // Stash the current permissions so that we can update the RapporService
  // correctly when the Rappor preference changes.  The metrics recording
  // preference partially determines the initial rappor setting, and also
  // controls whether FINE metrics are sent.
  may_record_ = may_record;
  may_upload_ = may_upload;

  metrics::MetricsService* metrics = GetMetricsService();

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  const bool only_do_metrics_recording =
      cmdline->HasSwitch(switches::kMetricsRecordingOnly) ||
      cmdline->HasSwitch(switches::kEnableBenchmarking);

  if (only_do_metrics_recording) {
    metrics->StartRecordingForTests();
    GetRapporService()->Update(rappor::FINE_LEVEL, false);
    return;
  }

  if (may_record) {
    if (!metrics->recording_active())
      metrics->Start();

    if (may_upload)
      metrics->EnableReporting();
    else
      metrics->DisableReporting();
  } else if (metrics->recording_active() || metrics->reporting_active()) {
    metrics->Stop();
  }

  rappor::RecordingLevel recording_level = rappor::RECORDING_DISABLED;
#if defined(GOOGLE_CHROME_BUILD)
  if (may_record)
    recording_level = rappor::FINE_LEVEL
#endif  // defined(GOOGLE_CHROME_BUILD)
  GetRapporService()->Update(recording_level, may_upload);
}

void MetricsServicesManager::UpdateUploadPermissions(bool may_upload) {
  return UpdatePermissions(
      ChromeMetricsServiceAccessor::IsMetricsReportingEnabled(), may_upload);
}
