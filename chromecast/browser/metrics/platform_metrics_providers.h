// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_PLATFORM_METRICS_PROVIDERS_H_
#define CHROMECAST_BROWSER_METRICS_PLATFORM_METRICS_PROVIDERS_H_

#include "components/metrics/proto/system_profile.pb.h"

namespace metrics {
class MetricsService;
}

namespace chromecast {

class CastService;

namespace metrics {

// Build-level hook for different platforms to provide data to MetricsService.
void RegisterPlatformMetricsProviders(
    ::metrics::MetricsService* metrics_service,
    CastService* cast_servce);

// Returns UMA client ID persisted in the platform.
const std::string GetPlatformClientID(CastService* cast_servce);

// Returns the current release channel.
::metrics::SystemProfileProto::Channel GetPlatformReleaseChannel(
     CastService* cast_servce);

// Returns a string representing this build's version.
std::string GetPlatformVersionString(CastService* cast_servce);

// Returns whether or not metrics reporting should be on.
bool PlatformIsReportingEnabled(CastService* cast_servce);

// Called when the UMA client ID has been set.
void PlatformSetClientID(CastService* cast_servce,
                         const std::string& client_id);

// Called when an upload has completed.
void PlatformOnLogUploadComplete(CastService* cast_servce);

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_PLATFORM_METRICS_PROVIDERS_H_
