// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/features.h"

namespace web {
namespace features {

bool StorePendingItemInContext() {
  if (base::FeatureList::IsEnabled(web::features::kSlimNavigationManager)) {
    // TODO(crbug.com/899827): Store Pending Item in NavigationContext with
    // slim-navigation-manager.
    return false;
  }
  return base::FeatureList::IsEnabled(kStorePendingItemInContext);
}

const base::Feature kIgnoresViewportScaleLimits{
    "IgnoresViewportScaleLimits", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSlimNavigationManager{"SlimNavigationManager",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kStorePendingItemInContext{
    "StorePendingItemInContext", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWKHTTPSystemCookieStore{"WKHTTPSystemCookieStore",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCrashOnUnexpectedURLChange{
    "CrashOnUnexpectedURLChange", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBrowserContainerFullscreen{
    "BrowserContainerFullscreen", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOutOfWebFullscreen{"OutOfWebFullscreen",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHistoryClobberWorkaround{
    "WKWebViewHistoryClobberWorkaround", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBlockUniversalLinksInOffTheRecordMode{
    "BlockUniversalLinksInOffTheRecord", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWebUISchemeHandling{"WebUISchemeHandling",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKeepsRenderProcessAlive{"KeepsRenderProcessAlive",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

bool WebUISchemeHandlingEnabled() {
  return base::FeatureList::IsEnabled(web::features::kWebUISchemeHandling);
}

}  // namespace features
}  // namespace web
