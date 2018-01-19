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
extern const base::Feature kAdjustWebApkInstallationSpace;
extern const base::Feature kAllowReaderForAccessibility;
extern const base::Feature kAndroidPayIntegrationV1;
extern const base::Feature kAndroidPayIntegrationV2;
extern const base::Feature kAndroidPaymentApps;
extern const base::Feature kCCTBackgroundTab;
extern const base::Feature kCCTExternalLinkHandling;
extern const base::Feature kCCTPostMessageAPI;
extern const base::Feature kCCTRedirectPreconnect;
extern const base::Feature kChromeHomeFeature;
extern const base::Feature kChromeHomeBottomNavLabels;
extern const base::Feature kChromeHomeClearUrlOnOpen;
extern const base::Feature kChromeHomeDestroySuggestions;
extern const base::Feature kChromeHomeDropAllButFirstThumbnail;
extern const base::Feature kChromeHomeInactivitySheetExpansion;
extern const base::Feature kChromeHomeMenuItemsExpandSheet;
extern const base::Feature kChromeHomePersistentIph;
extern const base::Feature kChromeHomePromo;
extern const base::Feature kChromeHomePromoInfoOnly;
extern const base::Feature kChromeHomePromoOnStartup;
extern const base::Feature kChromeHomePullToRefreshIphAtTop;
extern const base::Feature kChromeHomeOptOutSnackbar;
extern const base::Feature kChromeHomeShowGoogleGWhenUrlCleared;
extern const base::Feature kChromeHomeSurvey;
extern const base::Feature kChromeHomeSwipeLogic;
extern const base::Feature kChromeHomeSwipeLogicVelocity;
extern const base::Feature kChromeMemexFeature;
extern const base::Feature kChromeSmartSelection;
extern const base::Feature kContentSuggestionsScrollToLoad;
extern const base::Feature kContentSuggestionsSettings;
extern const base::Feature kContentSuggestionsThumbnailDominantColor;
extern const base::Feature kContextualSearchMlTapSuppression;
extern const base::Feature kContextualSearchSecondTap;
extern const base::Feature kContextualSearchTapDisableOverride;
extern const base::Feature kContextualSuggestionsAboveArticles;
extern const base::Feature kContextualSuggestionsCarousel;
extern const base::Feature kCustomContextMenu;
extern const base::Feature kCustomFeedbackUi;
extern const base::Feature kDontPrefetchLibraries;
extern const base::Feature kDownloadAutoResumptionThrottling;
extern const base::Feature kDownloadHomeShowStorageInfo;
extern const base::Feature kFullscreenActivity;
extern const base::Feature kImprovedA2HS;
extern const base::Feature kLanguagesPreference;
extern const base::Feature kSearchEnginePromoExistingDevice;
extern const base::Feature kSearchEnginePromoNewDevice;
extern const base::Feature kNewPhotoPicker;
extern const base::Feature kNoCreditCardAbort;
extern const base::Feature kNTPCondensedLayoutFeature;
extern const base::Feature kNTPCondensedTileLayoutFeature;
extern const base::Feature kNTPModernLayoutFeature;
extern const base::Feature kNTPLaunchAfterInactivity;
extern const base::Feature NTPShowGoogleGInOmniboxFeature;
extern const base::Feature kOmniboxSpareRenderer;
extern const base::Feature kPayWithGoogleV1;
extern const base::Feature kPhysicalWebFeature;
extern const base::Feature kPhysicalWebSharing;
extern const base::Feature kProgressBarThrottleFeature;
extern const base::Feature kPwaImprovedSplashScreen;
extern const base::Feature kPwaPersistentNotification;
extern const base::Feature kReaderModeInCCT;
extern const base::Feature kSoleIntegration;
extern const base::Feature kSpannableInlineAutocomplete;
extern const base::Feature kSpecialLocaleFeature;
extern const base::Feature kSpecialLocaleWrapper;
extern const base::Feature kTabModalJsDialog;
extern const base::Feature kTabReparenting;
extern const base::Feature kTrustedWebActivity;
extern const base::Feature kUserMediaScreenCapturing;
extern const base::Feature kVideoPersistence;
extern const base::Feature kVrBrowsingFeedback;
extern const base::Feature kVrBrowsingInCustomTab;
extern const base::Feature kVrLaunchIntents;
extern const base::Feature kWebPaymentsSingleAppUiSkip;
extern const base::Feature kWebVrAutopresentFromIntent;
extern const base::Feature kWebVrCardboardSupport;

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
