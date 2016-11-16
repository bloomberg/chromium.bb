// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/version.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"

namespace media_router {

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderWakeReason(
    MediaRouteProviderWakeReason reason) {
  DCHECK_LT(static_cast<int>(reason),
            static_cast<int>(MediaRouteProviderWakeReason::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Provider.WakeReason", static_cast<int>(reason),
      static_cast<int>(MediaRouteProviderWakeReason::TOTAL_COUNT));
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderVersion(
    const extensions::Extension& extension) {
  MediaRouteProviderVersion version = MediaRouteProviderVersion::UNKNOWN;
  const base::Version* extension_version = extension.version();
  if (extension_version) {
    version = GetMediaRouteProviderVersion(
        *extension_version, base::Version(version_info::GetVersionNumber()));
  }

  DCHECK_LT(static_cast<int>(version),
            static_cast<int>(MediaRouteProviderVersion::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Provider.Version", static_cast<int>(version),
      static_cast<int>(MediaRouteProviderVersion::TOTAL_COUNT));
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
    MediaRouteProviderWakeup wakeup) {
  DCHECK_LT(static_cast<int>(wakeup),
            static_cast<int>(MediaRouteProviderWakeup::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Provider.Wakeup", static_cast<int>(wakeup),
      static_cast<int>(MediaRouteProviderWakeup::TOTAL_COUNT));
}

// static
void MediaRouterMojoMetrics::RecordCreateRouteResultCode(
    RouteRequestResult::ResultCode result_code) {
  DCHECK_LT(result_code, RouteRequestResult::ResultCode::TOTAL_COUNT);
  UMA_HISTOGRAM_ENUMERATION("MediaRouter.Provider.CreateRoute.Result",
                            result_code,
                            RouteRequestResult::ResultCode::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordJoinRouteResultCode(
    RouteRequestResult::ResultCode result_code) {
  DCHECK_LT(result_code, RouteRequestResult::ResultCode::TOTAL_COUNT);
  UMA_HISTOGRAM_ENUMERATION("MediaRouter.Provider.JoinRoute.Result",
                            result_code,
                            RouteRequestResult::ResultCode::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderTerminateRoute(
    RouteRequestResult::ResultCode result_code) {
  DCHECK_LT(result_code, RouteRequestResult::ResultCode::TOTAL_COUNT);
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Provider.TerminateRoute.Result", result_code,
      RouteRequestResult::ResultCode::TOTAL_COUNT);
}

// static
MediaRouteProviderVersion MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
    const base::Version& extension_version,
    const base::Version& browser_version) {
  if (!extension_version.IsValid() || extension_version.components().empty() ||
      !browser_version.IsValid() || browser_version.components().empty()) {
    return MediaRouteProviderVersion::UNKNOWN;
  }

  uint32_t extension_major = extension_version.components()[0];
  uint32_t browser_major = browser_version.components()[0];
  // Sanity check.
  if (extension_major == 0 || browser_major == 0) {
    return MediaRouteProviderVersion::UNKNOWN;
  } else if (extension_major >= browser_major) {
    return MediaRouteProviderVersion::SAME_VERSION_AS_CHROME;
  } else if (browser_major - extension_major == 1) {
    return MediaRouteProviderVersion::ONE_VERSION_BEHIND_CHROME;
  } else {
    return MediaRouteProviderVersion::MULTIPLE_VERSIONS_BEHIND_CHROME;
  }
}

}  // namespace media_router
