// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics_services_manager/metrics_services_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/ukm/ukm_service.h"
#include "components/variations/service/variations_service.h"

namespace metrics_services_manager {

MetricsServicesManager::MetricsServicesManager(
    std::unique_ptr<MetricsServicesManagerClient> client)
    : client_(std::move(client)), may_upload_(false), may_record_(false) {
  DCHECK(client_);
}

MetricsServicesManager::~MetricsServicesManager() {}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
MetricsServicesManager::CreateEntropyProvider() {
  return client_->CreateEntropyProvider();
}

metrics::MetricsService* MetricsServicesManager::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetMetricsService();
}

rappor::RapporServiceImpl* MetricsServicesManager::GetRapporServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!rappor_service_) {
    rappor_service_ = client_->CreateRapporServiceImpl();
    rappor_service_->Initialize(client_->GetURLRequestContext());
  }
  return rappor_service_.get();
}

ukm::UkmService* MetricsServicesManager::GetUkmService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetUkmService();
}

variations::VariationsService* MetricsServicesManager::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!variations_service_)
    variations_service_ = client_->CreateVariationsService();
  return variations_service_.get();
}

void MetricsServicesManager::OnPluginLoadingError(
    const base::FilePath& plugin_path) {
  GetMetricsServiceClient()->OnPluginLoadingError(plugin_path);
}

void MetricsServicesManager::OnRendererProcessCrash() {
  GetMetricsServiceClient()->OnRendererProcessCrash();
}

metrics::MetricsServiceClient*
MetricsServicesManager::GetMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_service_client_) {
    metrics_service_client_ = client_->CreateMetricsServiceClient();
    // base::Unretained is safe since |this| owns the metrics_service_client_.
    metrics_service_client_->SetUpdateRunningServicesCallback(
        base::Bind(&MetricsServicesManager::UpdateRunningServices,
                   base::Unretained(this)));
  }
  return metrics_service_client_.get();
}

void MetricsServicesManager::UpdatePermissions(bool current_may_record,
                                               bool current_may_upload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If the user has opted out of metrics, delete local UKM state.
  if (may_record_ && !current_may_record) {
    ukm::UkmService* ukm = GetUkmService();
    if (ukm) {
      ukm->Purge();
      ukm->ResetClientId();
    }
  }

  // Stash the current permissions so that we can update the RapporServiceImpl
  // correctly when the Rappor preference changes.  The metrics recording
  // preference partially determines the initial rappor setting, and also
  // controls whether FINE metrics are sent.
  may_record_ = current_may_record;
  may_upload_ = current_may_upload;
  UpdateRunningServices();
}

void MetricsServicesManager::UpdateRunningServices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics::MetricsService* metrics = GetMetricsService();

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(metrics::switches::kMetricsRecordingOnly)) {
    metrics->StartRecordingForTests();
    GetRapporServiceImpl()->Update(true, false);
    return;
  }

  client_->UpdateRunningServices(may_record_, may_upload_);

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

  UpdateUkmService();

  GetRapporServiceImpl()->Update(may_record_, may_upload_);
}

void MetricsServicesManager::UpdateUkmService() {
  ukm::UkmService* ukm = GetUkmService();
  if (!ukm)
    return;
  bool sync_enabled =
      client_->IsMetricsReportingForceEnabled() ||
      metrics_service_client_->IsHistorySyncEnabledOnAllProfiles();
  bool is_incognito = client_->IsIncognitoSessionActive();
  if (may_record_ && sync_enabled & !is_incognito) {
    ukm->EnableRecording();
    if (may_upload_)
      ukm->EnableReporting();
    else
      ukm->DisableReporting();
  } else {
    ukm->DisableRecording();
    ukm->DisableReporting();
  }
}

void MetricsServicesManager::UpdateUploadPermissions(bool may_upload) {
  UpdatePermissions((client_->IsMetricsReportingForceEnabled() ||
                     client_->IsMetricsReportingEnabled()),
                    client_->IsMetricsReportingForceEnabled() || may_upload);
}

}  // namespace metrics_services_manager
