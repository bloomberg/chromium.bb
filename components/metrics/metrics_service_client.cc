// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_client.h"

#include "base/feature_list.h"
#include "components/metrics/url_constants.h"

namespace metrics {

namespace {

#if defined(OS_ANDROID) || defined(OS_IOS)
const base::Feature kNewMetricsUrlFeature{"NewMetricsUrl",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kNewMetricsUrlFeature{"NewMetricsUrl",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace

MetricsServiceClient::MetricsServiceClient() : update_running_services_() {}

MetricsServiceClient::~MetricsServiceClient() {}

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
  return base::FeatureList::IsEnabled(kNewMetricsUrlFeature)
             ? kNewMetricsServerUrl
             : kOldMetricsServerUrl;
}

bool MetricsServiceClient::IsHistorySyncEnabledOnAllProfiles() {
  return false;
}

void MetricsServiceClient::SetUpdateRunningServicesCallback(
    const base::Closure& callback) {
  update_running_services_ = callback;
}

void MetricsServiceClient::UpdateRunningServices() {
  if (update_running_services_)
    update_running_services_.Run();
}

}  // namespace metrics
