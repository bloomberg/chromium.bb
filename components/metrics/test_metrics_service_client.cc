// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test_metrics_service_client.h"

namespace metrics {

TestMetricsServiceClient::TestMetricsServiceClient() {
}

TestMetricsServiceClient::~TestMetricsServiceClient() {
}

void TestMetricsServiceClient::SetClientID(const std::string& client_id) {
  client_id_ = client_id;
}

bool TestMetricsServiceClient::IsOffTheRecordSessionActive() {
  return false;
}

std::string TestMetricsServiceClient::GetApplicationLocale() {
  return "en-US";
}

bool TestMetricsServiceClient::GetBrand(std::string* brand_code) {
  *brand_code = "BRAND_CODE";
  return true;
}

SystemProfileProto::Channel TestMetricsServiceClient::GetChannel() {
  return SystemProfileProto::CHANNEL_BETA;
}

std::string TestMetricsServiceClient::GetVersionString() {
  return "5.0.322.0-64-devel";
}

void TestMetricsServiceClient::OnLogUploadComplete() {
}

}  // namespace metrics
