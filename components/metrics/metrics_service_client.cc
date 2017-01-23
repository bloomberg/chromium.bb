// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_client.h"

#include "components/metrics/url_constants.h"

namespace metrics {

ukm::UkmService* MetricsServiceClient::GetUkmService() {
  return nullptr;
}

base::string16 MetricsServiceClient::GetRegistryBackupKey() {
  return base::string16();
}

bool MetricsServiceClient::IsReportingPolicyManaged() {
  return false;
}

EnableMetricsDefault MetricsServiceClient::GetMetricsReportingDefaultState() {
  return EnableMetricsDefault::DEFAULT_UNKNOWN;
}

bool MetricsServiceClient::IsUMACellularUploadLogicEnabled() {
  return false;
}

std::string MetricsServiceClient::GetMetricsServerUrl() {
  return metrics::kDefaultMetricsServerUrl;
}

}  // namespace metrics
