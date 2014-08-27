// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/platform_metrics_providers.h"

namespace chromecast {
namespace metrics {

void RegisterPlatformMetricsProviders(
    ::metrics::MetricsService* metrics_service) {
}

::metrics::SystemProfileProto::Channel GetPlatformReleaseChannel() {
  return ::metrics::SystemProfileProto::CHANNEL_STABLE;
}

std::string GetPlatformVersionString() {
  return "";
}

bool PlatformIsReportingEnabled() {
  return false;
}

void PlatformSetClientID(const std::string& client_id) {
}

void PlatformOnLogUploadComplete() {
}

}  // namespace metrics
}  // namespace chromecast
