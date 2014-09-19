// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_metrics_service_client.h"

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "chromecast/common/chromecast_config.h"
#include "chromecast/common/chromecast_switches.h"
#include "chromecast/metrics/platform_metrics_providers.h"
#include "components/metrics/client_info.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"

namespace chromecast {
namespace metrics {

namespace {

void StoreClientInfo(const ::metrics::ClientInfo& client_info) {
}

scoped_ptr<::metrics::ClientInfo> LoadClientInfo() {
  return scoped_ptr<::metrics::ClientInfo>();
}

}  // namespace

// static
CastMetricsServiceClient* CastMetricsServiceClient::Create(
    base::TaskRunner* io_task_runner,
    PrefService* pref_service,
    net::URLRequestContextGetter* request_context) {
  return new CastMetricsServiceClient(io_task_runner,
                                      pref_service,
                                      request_context);
}

void CastMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  LOG(INFO) << "Metrics client ID set: " << client_id;
  PlatformSetClientID(client_id);
}

bool CastMetricsServiceClient::IsOffTheRecordSessionActive() {
  // Chromecast behaves as "off the record" w/r/t recording browsing state,
  // but this value is about not disabling metrics because of it.
  return false;
}

std::string CastMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

bool CastMetricsServiceClient::GetBrand(std::string* brand_code) {
  return false;
}

::metrics::SystemProfileProto::Channel CastMetricsServiceClient::GetChannel() {
  return GetPlatformReleaseChannel();
}

std::string CastMetricsServiceClient::GetVersionString() {
  return GetPlatformVersionString();
}

void CastMetricsServiceClient::OnLogUploadComplete() {
  PlatformOnLogUploadComplete();
}

void CastMetricsServiceClient::StartGatheringMetrics(
    const base::Closure& done_callback) {
  done_callback.Run();
}

void CastMetricsServiceClient::CollectFinalMetrics(
    const base::Closure& done_callback) {
  done_callback.Run();
}

scoped_ptr< ::metrics::MetricsLogUploader>
CastMetricsServiceClient::CreateUploader(
    const std::string& server_url,
    const std::string& mime_type,
    const base::Callback<void(int)>& on_upload_complete) {
  std::string uma_server_url(server_url);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOverrideMetricsUploadUrl)) {
    uma_server_url.assign(
        command_line->GetSwitchValueASCII(switches::kOverrideMetricsUploadUrl));
  }
  DCHECK(!uma_server_url.empty());
  return scoped_ptr< ::metrics::MetricsLogUploader>(
      new ::metrics::NetMetricsLogUploader(
          request_context_,
          uma_server_url,
          mime_type,
          on_upload_complete));
}

void CastMetricsServiceClient::EnableMetricsService(bool enabled) {
  if (enabled) {
    metrics_service_->Start();
  } else {
    metrics_service_->Stop();
  }
}

CastMetricsServiceClient::CastMetricsServiceClient(
    base::TaskRunner* io_task_runner,
    PrefService* pref_service,
    net::URLRequestContextGetter* request_context)
    : metrics_state_manager_(::metrics::MetricsStateManager::Create(
          pref_service,
          base::Bind(&CastMetricsServiceClient::IsReportingEnabled,
                     base::Unretained(this)),
                     base::Bind(&StoreClientInfo),
                     base::Bind(&LoadClientInfo))),
      metrics_service_(new ::metrics::MetricsService(
          metrics_state_manager_.get(),
          this,
          pref_service)),
      request_context_(request_context) {
  // Always create a client id as it may also be used by crash reporting,
  // (indirectly) included in feedback, and can be queried during setup.
  // For UMA and crash reporting, associated opt-in settings will control
  // sending reports as directed by the user.
  // For Setup (which also communicates the user's opt-in preferences),
  // report the client-id and expect that setup will handle the current opt-in
  // value.
  metrics_state_manager_->ForceClientIdCreation();

  metrics_service_->RegisterMetricsProvider(
      scoped_ptr< ::metrics::MetricsProvider>(
          new ::metrics::GPUMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr< ::metrics::MetricsProvider>(
          new NetworkMetricsProvider(io_task_runner)));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr< ::metrics::MetricsProvider>(
          new ::metrics::ProfilerMetricsProvider));
  RegisterPlatformMetricsProviders(metrics_service_.get());

  metrics_service_->InitializeMetricsRecordingState();

  if (IsReportingEnabled())
    metrics_service_->Start();
}

CastMetricsServiceClient::~CastMetricsServiceClient() {
}

bool CastMetricsServiceClient::IsReportingEnabled() {
  return PlatformIsReportingEnabled();
}

}  // namespace metrics
}  // namespace chromecast
