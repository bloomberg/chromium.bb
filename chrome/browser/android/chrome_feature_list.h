// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_

#include <jni.h>

#include "base/feature_list.h"

namespace chrome {
namespace android {

// Alphabetical:
extern const base::Feature kAllowReaderForAccessibility;
extern const base::Feature kAndroidPayIntegrationV1;
extern const base::Feature kAndroidPayIntegrationV2;
extern const base::Feature kAndroidPaymentApps;
extern const base::Feature kAndroidSigninPromos;
extern const base::Feature kCCTBackgroundTab;
extern const base::Feature kCCTExternalLinkHandling;
extern const base::Feature kCCTPostMessageAPI;
extern const base::Feature kCCTRedirectPreconnect;
extern const base::Feature kChromeHomeFeature;
extern const base::Feature kChromeHomeDestroySuggestions;
extern const base::Feature kChromeHomeBottomNavLabels;
extern const base::Feature kChromeHomeDoodle;
extern const base::Feature kChromeHomePromo;
extern const base::Feature kChromeHomeOptOutSnackbar;
extern const base::Feature kChromeHomeSwipeLogic;
extern const base::Feature kChromeMemexFeature;
extern const base::Feature kContentSuggestionsLargeThumbnail;
extern const base::Feature kContentSuggestionsScrollToLoad;
extern const base::Feature kContentSuggestionsSettings;
extern const base::Feature kContentSuggestionsThumbnailDominantColor;
extern const base::Feature kContextualSearchSingleActions;
extern const base::Feature kContextualSearchUrlActions;
extern const base::Feature kContextualSuggestionsCarousel;
extern const base::Feature kCustomContextMenu;
extern const base::Feature kCustomFeedbackUi;
extern const base::Feature kDontPrefetchLibraries;
extern const base::Feature kDownloadAutoResumptionThrottling;
extern const base::Feature kDownloadHomeShowStorageInfo;
extern const base::Feature kFullscreenActivity;
extern const base::Feature kImprovedA2HS;
extern const base::Feature kSearchEnginePromoExistingDevice;
extern const base::Feature kSearchEnginePromoNewDevice;
extern const base::Feature kNewPhotoPicker;
extern const base::Feature kNoCreditCardAbort;
extern const base::Feature kNTPCondensedLayoutFeature;
extern const base::Feature kNTPCondensedTileLayoutFeature;
extern const base::Feature kNTPLaunchAfterInactivity;
extern const base::Feature kNTPOfflinePagesFeature;
extern const base::Feature NTPShowGoogleGInOmniboxFeature;
extern const base::Feature kOmniboxSpareRenderer;
extern const base::Feature kPayWithGoogleV1;
extern const base::Feature kPhysicalWebFeature;
extern const base::Feature kPhysicalWebSharing;
extern const base::Feature kProgressBarThrottleFeature;
extern const base::Feature kPwaImprovedSplashScreen;
extern const base::Feature kPwaPersistentNotification;
extern const base::Feature kReaderModeInCCT;
extern const base::Feature kSpannableInlineAutocomplete;
extern const base::Feature kSpecialLocaleFeature;
extern const base::Feature kSpecialLocaleWrapper;
extern const base::Feature kTabModalJsDialog;
extern const base::Feature kTabReparenting;
extern const base::Feature kUserMediaScreenCapturing;
extern const base::Feature kVideoPersistence;
extern const base::Feature kVrBrowsingFeedback;
extern const base::Feature kVrCustomTabBrowsing;
extern const base::Feature kWebPaymentsSingleAppUiSkip;
extern const base::Feature kWebVrAutopresent;
extern const base::Feature kWebVRCardboardSupport;

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
