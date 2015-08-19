// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/metrics/platform_metrics_providers.h"

namespace chromecast {
namespace metrics {

void RegisterPlatformMetricsProviders(
    ::metrics::MetricsService* metrics_service,
    CastService* cast_servce) {
}

const std::string GetPlatformClientID(CastService* cast_servce) {
  return "";
}

::metrics::SystemProfileProto::Channel GetPlatformReleaseChannel(
     CastService* cast_servce) {
  return ::metrics::SystemProfileProto::CHANNEL_STABLE;
}

void PlatformSetClientID(CastService* cast_servce,
                         const std::string& client_id) {
}

}  // namespace metrics
}  // namespace chromecast
