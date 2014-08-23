// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_metrics_service_client.h"

#include "base/i18n/rtl.h"
#include "chromecast/common/chromecast_config.h"
#include "chromecast/metrics/platform_metrics_providers.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/net_metrics_log_uploader.h"

namespace chromecast {
namespace metrics {

// static
CastMetricsServiceClient* CastMetricsServiceClient::Create(
    PrefService* pref_service,
    net::URLRequestContextGetter* request_context) {
  return new CastMetricsServiceClient(pref_service, request_context);
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
  return scoped_ptr< ::metrics::MetricsLogUploader>(
      new ::metrics::NetMetricsLogUploader(
          request_context_,
          server_url,
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
    PrefService* pref_service,
    net::URLRequestContextGetter* request_context)
    : metrics_state_manager_(::metrics::MetricsStateManager::Create(
          pref_service,
          base::Bind(&CastMetricsServiceClient::IsReportingEnabled,
                     base::Unretained(this)),
                     ::metrics::MetricsStateManager::StoreClientInfoCallback(),
                     ::metrics::MetricsStateManager::LoadClientInfoCallback())),
      metrics_service_(new MetricsService(
          metrics_state_manager_.get(),
          this,
          ChromecastConfig::GetInstance()->pref_service())),
      request_context_(request_context) {
  // Always create a client id as it may also be used by crash reporting,
  // (indirectly) included in feedback, and can be queried during setup.
  // For UMA and crash reporting, associated opt-in settings will control
  // sending reports as directed by the user.
  // For Setup (which also communicates the user's opt-in preferences),
  // report the client-id and expect that setup will handle the current opt-in
  // value.
  metrics_state_manager_->ForceClientIdCreation();

  // TODO(gunsch): Add the following: GPUMetricsProvider,
  // NetworkMetricsProvider, ProfilerMetricsProvider. See: crbug/404791
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
