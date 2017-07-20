// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
#define IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_

#include <string>

// This file can be empty. Its purpose is to contain the relatively short lived
// declarations required for experimental flags.

namespace experimental_flags {

enum GaiaEnvironment {
  GAIA_ENVIRONMENT_PROD,
  GAIA_ENVIRONMENT_STAGING,
  GAIA_ENVIRONMENT_TEST,
};

enum WhatsNewPromoStatus {
  WHATS_NEW_DEFAULT = 0,         // Not forced to enable a promo.
  WHATS_NEW_APP_RATING,          // Force enable App Rating Promo.
  WHATS_NEW_MOVE_TO_DOCK_TIP,    // Force enable Move To Dock Tip promo.
  WHATS_NEW_PROMO_STATUS_COUNT,  // Count of Whats New Promo Statuses.
};

// Whether the First Run UI will be always be displayed.
bool AlwaysDisplayFirstRun();

GaiaEnvironment GetGaiaEnvironment();

// Returns the host name for an alternative Origin Server host for use by
// |BrandCode| startup ping. Returns empty string if there is no alternative
// host specified.
std::string GetOriginServerHost();

// Returns the promo force enabled, as determined by the experimental flags.
// If |WHATS_NEW_DEFAULT| is returned, no promo is force enabled.
WhatsNewPromoStatus GetWhatsNewPromoStatus();

// Whether background crash report upload should generate a local notification.
bool IsAlertOnBackgroundUploadEnabled();

// Whether auto-reload is enabled.
bool IsAutoReloadEnabled();

// Whether the lru snapshot cache experiment is enabled.
bool IsLRUSnapshotCacheEnabled();

// Whether the iOS MDM integration is enabled.
bool IsMDMIntegrationEnabled();

// Whether memory debugging tools are enabled.
bool IsMemoryDebuggingEnabled();

// Whether the Clear Browsing Data counters and time selection UI is enabled.
bool IsNewClearBrowsingDataUIEnabled();

// Whether the page icon for downgraded HTTPS is enabled.
bool IsPageIconForDowngradedHTTPSEnabled();

// Whether password generation is enabled.
bool IsPasswordGenerationEnabled();

// Whether the Physical Web feature is enabled.
bool IsPhysicalWebEnabled();

// Whether reader mode is enabled.
bool IsReaderModeEnabled();

// Whether request mobile site is enabled.
bool IsRequestMobileSiteEnabled();

// Whether the Sign In Flow via SFSafariViewController is enabled.
bool IsSafariVCSignInEnabled();

// Whether startup crash is enabled.
bool IsStartupCrashEnabled();

// Whether or not the tab strip scrolls new tabs to be visible.
bool IsTabStripAutoScrollNewTabsEnabled();

// Whether viewing and copying passwords is enabled.
bool IsViewCopyPasswordsEnabled();

// Whether password generation fields are determined using local heuristics
// only.
bool UseOnlyLocalHeuristicsForPasswordGeneration();

// Whether the Suggestions UI is enabled.
bool IsSuggestionsUIEnabled();

// Whether Sign-in promo is enabled.
bool IsSigninPromoEnabled();

// Whether Bookmark reordering is enabled.
bool IsBookmarkReorderingEnabled();

// Whether a new version of FeedbackKit is the preferred feedback UI provider.
bool IsNewFeedbackKitEnabled();

// Whether the keyboard accessory view with camera search is enabled.
bool IsKeyboardAccessoryViewWithCameraSearchEnabled();

// Whether the WKBackForwardList based navigation manager is enabled.
bool IsSlimNavigationManagerEnabled();

// Whether the 3rd party keyboard omnibox workaround is enabled.
bool IsThirdPartyKeyboardWorkaroundEnabled();

}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
