// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/feature_engagement/buildflags.h"

namespace feature_engagement {

// A feature for enabling a demonstration mode for In-Product Help (IPH).
extern const base::Feature kIPHDemoMode;

// A feature to ensure all arrays can contain at least one feature.
extern const base::Feature kIPHDummyFeature;

// All the features declared for Android below that are also used in Java,
// should also be declared in:
// org.chromium.components.feature_engagement.FeatureConstants.
#if defined(OS_ANDROID)
extern const base::Feature kIPHDataSaverDetailFeature;
extern const base::Feature kIPHDataSaverPreviewFeature;
extern const base::Feature kIPHDownloadHomeFeature;
extern const base::Feature kIPHDownloadPageFeature;
extern const base::Feature kIPHDownloadPageScreenshotFeature;
extern const base::Feature kIPHChromeHomeExpandFeature;
extern const base::Feature kIPHChromeHomeMenuHeaderFeature;
extern const base::Feature kIPHChromeHomePullToRefreshFeature;
extern const base::Feature kIPHMediaDownloadFeature;
extern const base::Feature kIPHContextualSearchWebSearchFeature;
extern const base::Feature kIPHContextualSearchPromoteTapFeature;
extern const base::Feature kIPHContextualSearchPromotePanelOpenFeature;
extern const base::Feature kIPHContextualSearchOptInFeature;
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
extern const base::Feature kIPHBookmarkFeature;
extern const base::Feature kIPHIncognitoWindowFeature;
extern const base::Feature kIPHNewTabFeature;
#endif  // BUILDFLAG(ENABLE_DESKTOP_IPH)

#if defined(OS_IOS)
extern const base::Feature kIPHNewTabTipFeature;
extern const base::Feature kIPHNewIncognitoTabTipFeature;
extern const base::Feature kIPHBadgedReadingListFeature;
#endif  // defined(OS_IOS)

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
