// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a list of the preferences that the
// PreferencesChangeProcessor should process changes for.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCHRONIZED_PREFERENCES_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCHRONIZED_PREFERENCES_H_

#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"

namespace browser_sync {

static const wchar_t* kSynchronizedPreferences[] = {
  // General profile preferences.
  prefs::kAcceptLanguages,
  prefs::kAlternateErrorPagesEnabled,
  prefs::kApplicationLocale,
  prefs::kAutoFillAuxiliaryProfilesEnabled,
  prefs::kAutoFillEnabled,
  prefs::kAutoFillInfoBarShown,
  prefs::kBlockThirdPartyCookies,
  prefs::kClearSiteDataOnExit,
  prefs::kCookiePromptExpanded,
  prefs::kDefaultCharset,
  prefs::kDefaultContentSettings,
  prefs::kDefaultSearchProviderID,
  prefs::kDefaultSearchProviderName,
  prefs::kDefaultSearchProviderSearchURL,
  prefs::kDefaultSearchProviderSuggestURL,
  prefs::kDeleteBrowsingHistory,
  prefs::kDeleteCache,
  prefs::kDeleteCookies,
  prefs::kDeleteDownloadHistory,
  prefs::kDeleteFormData,
  prefs::kDeletePasswords,
  prefs::kDeleteTimePeriod,
  prefs::kDesktopNotificationAllowedOrigins,
  prefs::kDesktopNotificationDeniedOrigins,
  prefs::kDnsPrefetchingEnabled,
  prefs::kEnableAutoSpellCorrect,
  prefs::kEnableSpellCheck,
  prefs::kEnableTranslate,
  prefs::kExtensionsUIDeveloperMode,
  prefs::kGeolocationContentSettings,
  prefs::kGeolocationDefaultContentSetting,
  prefs::kHomePage,
  prefs::kHomePageIsNewTabPage,
  prefs::kMixedContentFiltering,
  prefs::kNTPPromoImageRemaining,
  prefs::kNTPPromoLineRemaining,
  prefs::kPasswordManagerEnabled,
  prefs::kContentSettingsPatterns,
  prefs::kPerHostZoomLevels,
  prefs::kPinnedTabs,
  prefs::kPrintingPageFooterCenter,
  prefs::kPrintingPageFooterLeft,
  prefs::kPrintingPageFooterRight,
  prefs::kPrintingPageHeaderCenter,
  prefs::kPrintingPageHeaderLeft,
  prefs::kPrintingPageHeaderRight,
  prefs::kPrivacyFilterRules,
  prefs::kPromptForDownload,
  prefs::kRecentlySelectedEncoding,
  prefs::kRestoreOnStartup,
  prefs::kSafeBrowsingEnabled,
  prefs::kSearchSuggestEnabled,
  prefs::kShowBookmarkBar,
  prefs::kShowHomeButton,
  prefs::kShowOmniboxSearchHint,
  prefs::kShowPageOptionsButtons,
  prefs::kStaticEncodings,
  prefs::kURLsToRestoreOnStartup,
  prefs::kURLsToRestoreOnStartup,
  prefs::kWebKitDomPasteEnabled,
  prefs::kWebKitInspectorSettings,
  prefs::kWebKitJavaEnabled,
  prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
  prefs::kWebKitJavascriptEnabled,
  prefs::kWebKitLoadsImagesAutomatically,
  prefs::kWebKitPluginsEnabled,
  prefs::kWebKitShrinksStandaloneImagesToFit,
  prefs::kWebKitTextAreasAreResizable,
  prefs::kWebKitUsesUniversalDetector,
  prefs::kWebKitWebSecurityEnabled,

  // Translate preferences.
  TranslatePrefs::kPrefTranslateLanguageBlacklist,
  TranslatePrefs::kPrefTranslateSiteBlacklist,
  TranslatePrefs::kPrefTranslateWhitelists,
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCHRONIZED_PREFERENCES_H_
