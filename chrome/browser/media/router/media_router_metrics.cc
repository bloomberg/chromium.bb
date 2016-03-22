// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#if !defined(OS_ANDROID)
#include "base/version.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#endif  // !defined(OS_ANDROID)

namespace media_router {

// static
void MediaRouterMetrics::RecordMediaRouterDialogOrigin(
    MediaRouterDialogOpenOrigin origin) {
  DCHECK_LT(static_cast<int>(origin),
            static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Icon.Click.Location", static_cast<int>(origin),
      static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogPaint(
    const base::TimeDelta delta) {
  UMA_HISTOGRAM_TIMES("MediaRouter.Ui.Dialog.Paint",
                      delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogLoaded(
    const base::TimeDelta delta) {
  UMA_HISTOGRAM_TIMES("MediaRouter.Ui.Dialog.LoadedWithData",
                      delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterInitialUserAction(
    MediaRouterUserAction action) {
  DCHECK_LT(static_cast<int>(action),
            static_cast<int>(MediaRouterUserAction::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Ui.FirstAction", static_cast<int>(action),
      static_cast<int>(MediaRouterUserAction::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordRouteCreationOutcome(
    MediaRouterRouteCreationOutcome outcome) {
  DCHECK_LT(static_cast<int>(outcome),
            static_cast<int>(MediaRouterRouteCreationOutcome::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
    "MediaRouter.Route.CreationOutcome", static_cast<int>(outcome),
    static_cast<int>(MediaRouterRouteCreationOutcome::TOTAL_COUNT));
}

#if !defined(OS_ANDROID)
// static
void MediaRouterMetrics::RecordMediaRouteProviderWakeReason(
    MediaRouteProviderWakeReason reason) {
  DCHECK_LT(static_cast<int>(reason),
            static_cast<int>(MediaRouteProviderWakeReason::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Provider.WakeReason", static_cast<int>(reason),
      static_cast<int>(MediaRouteProviderWakeReason::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouteProviderVersion(
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
void MediaRouterMetrics::RecordMediaRouteProviderWakeup(
    MediaRouteProviderWakeup wakeup) {
  DCHECK_LT(static_cast<int>(wakeup),
            static_cast<int>(MediaRouteProviderWakeup::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Provider.Wakeup", static_cast<int>(wakeup),
      static_cast<int>(MediaRouteProviderWakeup::TOTAL_COUNT));
}

// static
MediaRouteProviderVersion MediaRouterMetrics::GetMediaRouteProviderVersion(
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
#endif  // !defined(OS_ANDROID)

}  // namespace media_router
