// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test_metrics_service_client.h"

#include "base/callback.h"
#include "components/metrics/metrics_log_uploader.h"

namespace metrics {

// static
const char TestMetricsServiceClient::kBrandForTesting[] = "brand_for_testing";

TestMetricsServiceClient::TestMetricsServiceClient()
    : version_string_("5.0.322.0-64-devel") {
}

TestMetricsServiceClient::~TestMetricsServiceClient() {
}

void TestMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  client_id_ = client_id;
}

bool TestMetricsServiceClient::IsOffTheRecordSessionActive() {
  return false;
}

std::string TestMetricsServiceClient::GetApplicationLocale() {
  return "en-US";
}

bool TestMetricsServiceClient::GetBrand(std::string* brand_code) {
  *brand_code = kBrandForTesting;
  return true;
}

SystemProfileProto::Channel TestMetricsServiceClient::GetChannel() {
  return SystemProfileProto::CHANNEL_BETA;
}

std::string TestMetricsServiceClient::GetVersionString() {
  return version_string_;
}

void TestMetricsServiceClient::OnLogUploadComplete() {
}

void TestMetricsServiceClient::StartGatheringMetrics(
    const base::Closure& done_callback) {
  done_callback.Run();
}

void TestMetricsServiceClient::CollectFinalMetrics(
    const base::Closure& done_callback) {
  done_callback.Run();
}

scoped_ptr<MetricsLogUploader> TestMetricsServiceClient::CreateUploader(
    const std::string& server_url,
    const std::string& mime_type,
    const base::Callback<void(int)>& on_upload_complete) {
  return scoped_ptr<MetricsLogUploader>();
}

}  // namespace metrics
