// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_PLATFORM_METRICS_PROVIDERS_H_
#define CHROMECAST_METRICS_PLATFORM_METRICS_PROVIDERS_H_

#include "components/metrics/proto/system_profile.pb.h"

namespace metrics {
class MetricsService;
}

namespace chromecast {
namespace metrics {

// Build-level hook for different platforms to provide data to MetricsService.
void RegisterPlatformMetricsProviders(
    ::metrics::MetricsService* metrics_service);

// Returns the current release channel.
::metrics::SystemProfileProto::Channel GetPlatformReleaseChannel();

// Returns a string representing this build's version.
std::string GetPlatformVersionString();

// Returns whether or not metrics reporting should be on.
bool PlatformIsReportingEnabled();

// Called when the UMA client ID has been set.
void PlatformSetClientID(const std::string& client_id);

// Called when an upload has completed.
void PlatformOnLogUploadComplete();

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_METRICS_PLATFORM_METRICS_PROVIDERS_H_
