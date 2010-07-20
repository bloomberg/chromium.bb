// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various preferences, for easier changing.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_

#include "build/build_config.h"

namespace prefs {

// Profile prefs
extern const wchar_t kHomePageIsNewTabPage[];
extern const wchar_t kHomePage[];
extern const wchar_t kSessionExitedCleanly[];
extern const wchar_t kRestoreOnStartup[];
extern const wchar_t kURLsToRestoreOnStartup[];
extern const wchar_t kApplicationLocale[];
extern const wchar_t kDefaultCharset[];
extern const wchar_t kAcceptLanguages[];
extern const wchar_t kStaticEncodings[];
extern const wchar_t kPopupWhitelistedHosts[];
extern const wchar_t kShowBookmarkBar[];
extern const wchar_t kWebKitStandardFontIsSerif[];
extern const wchar_t kWebKitFixedFontFamily[];
extern const wchar_t kWebKitSerifFontFamily[];
extern const wchar_t kWebKitSansSerifFontFamily[];
extern const wchar_t kWebKitCursiveFontFamily[];
extern const wchar_t kWebKitFantasyFontFamily[];
extern const wchar_t kWebKitDefaultFontSize[];
extern const wchar_t kWebKitDefaultFixedFontSize[];
extern const wchar_t kWebKitMinimumFontSize[];
extern const wchar_t kWebKitMinimumLogicalFontSize[];
extern const wchar_t kWebKitJavascriptEnabled[];
extern const wchar_t kWebKitWebSecurityEnabled[];
extern const wchar_t kWebKitJavascriptCanOpenWindowsAutomatically[];
extern const wchar_t kWebKitLoadsImagesAutomatically[];
extern const wchar_t kWebKitPluginsEnabled[];
extern const wchar_t kWebKitDomPasteEnabled[];
extern const wchar_t kWebKitShrinksStandaloneImagesToFit[];
extern const wchar_t kWebKitInspectorSettings[];
extern const wchar_t kWebKitUsesUniversalDetector[];
extern const wchar_t kWebKitTextAreasAreResizable[];
extern const wchar_t kWebKitJavaEnabled[];
extern const wchar_t kWebkitTabsToLinks[];
extern const wchar_t kPasswordManagerEnabled[];
extern const wchar_t kFormAutofillEnabled[];  // OBSOLETE
extern const wchar_t kSafeBrowsingEnabled[];
extern const wchar_t kSearchSuggestEnabled[];
extern const wchar_t kCookieBehavior[];  // OBSOLETE
extern const wchar_t kDefaultSearchProviderSearchURL[];
extern const wchar_t kDefaultSearchProviderSuggestURL[];
extern const wchar_t kDefaultSearchProviderName[];
extern const wchar_t kDefaultSearchProviderID[];
extern const wchar_t kDefaultSearchProviderPrepopulateID[];
extern const wchar_t kSearchProviderOverrides[];
extern const wchar_t kSearchProviderOverridesVersion[];
extern const wchar_t kPromptForDownload[];
extern const wchar_t kAlternateErrorPagesEnabled[];
extern const wchar_t kDnsPrefetchingEnabled[];
extern const wchar_t kDnsStartupPrefetchList[];
extern const wchar_t kDnsHostReferralList[];
extern const wchar_t kCookiePromptExpanded[];
#if defined(USE_NSS)
extern const wchar_t kCertRevocationCheckingEnabled[];
extern const wchar_t kSSL2Enabled[];
extern const wchar_t kSSL3Enabled[];
extern const wchar_t kTLS1Enabled[];
#endif
#if defined(OS_CHROMEOS)
extern const wchar_t kTapToClickEnabled[];
extern const wchar_t kVertEdgeScrollEnabled[];
extern const wchar_t kTouchpadSpeedFactor[];
extern const wchar_t kTouchpadSensitivity[];
extern const wchar_t kLanguageCurrentInputMethod[];
extern const wchar_t kLanguagePreviousInputMethod[];
extern const wchar_t kLanguageHotkeyNextEngineInMenu[];
extern const wchar_t kLanguageHotkeyPreviousEngine[];
extern const wchar_t kLanguagePreferredLanguages[];
extern const wchar_t kLanguagePreloadEngines[];
extern const wchar_t kLanguageChewingAutoShiftCur[];
extern const wchar_t kLanguageChewingAddPhraseDirection[];
extern const wchar_t kLanguageChewingEasySymbolInput[];
extern const wchar_t kLanguageChewingEscCleanAllBuf[];
extern const wchar_t kLanguageChewingForceLowercaseEnglish[];
extern const wchar_t kLanguageChewingPlainZhuyin[];
extern const wchar_t kLanguageChewingPhraseChoiceRearward[];
extern const wchar_t kLanguageChewingSpaceAsSelection[];
extern const wchar_t kLanguageChewingMaxChiSymbolLen[];
extern const wchar_t kLanguageChewingCandPerPage[];
extern const wchar_t kLanguageChewingKeyboardType[];
extern const wchar_t kLanguageChewingSelKeys[];
extern const wchar_t kLanguageChewingHsuSelKeyType[];
extern const wchar_t kLanguageHangulKeyboard[];
extern const wchar_t kLanguageHangulHanjaKeys[];
extern const wchar_t kLanguagePinyinCorrectPinyin[];
extern const wchar_t kLanguagePinyinFuzzyPinyin[];
extern const wchar_t kLanguagePinyinLookupTablePageSize[];
extern const wchar_t kLanguagePinyinShiftSelectCandidate[];
extern const wchar_t kLanguagePinyinMinusEqualPage[];
extern const wchar_t kLanguagePinyinCommaPeriodPage[];
extern const wchar_t kLanguagePinyinAutoCommit[];
extern const wchar_t kLanguagePinyinDoublePinyin[];
extern const wchar_t kLanguagePinyinDoublePinyinSchema[];
extern const wchar_t kLanguagePinyinInitChinese[];
extern const wchar_t kLanguagePinyinInitFull[];
extern const wchar_t kLanguagePinyinInitFullPunct[];
extern const wchar_t kLanguagePinyinInitSimplifiedChinese[];
extern const wchar_t kLanguagePinyinTradCandidate[];
extern const wchar_t kLanguageMozcPreeditMethod[];
extern const wchar_t kLanguageMozcSessionKeymap[];
extern const wchar_t kLanguageMozcPunctuationMethod[];
extern const wchar_t kLanguageMozcSymbolMethod[];
extern const wchar_t kLanguageMozcSpaceCharacterForm[];
extern const wchar_t kLanguageMozcHistoryLearningLevel[];
extern const wchar_t kLanguageMozcSelectionShortcut[];
extern const wchar_t kLanguageMozcShiftKeyModeSwitch[];
extern const wchar_t kLanguageMozcNumpadCharacterForm[];
extern const wchar_t kLanguageMozcIncognitoMode[];
extern const wchar_t kLanguageMozcUseAutoImeTurnOff[];
extern const wchar_t kLanguageMozcUseDateConversion[];
extern const wchar_t kLanguageMozcUseSingleKanjiConversion[];
extern const wchar_t kLanguageMozcUseSymbolConversion[];
extern const wchar_t kLanguageMozcUseNumberConversion[];
extern const wchar_t kLanguageMozcUseHistorySuggest[];
extern const wchar_t kLanguageMozcUseDictionarySuggest[];
extern const wchar_t kLanguageMozcSuggestionsSize[];
extern const wchar_t kAccessibilityEnabled[];
extern const wchar_t kLabsAdvancedFilesystemEnabled[];
extern const wchar_t kLabsMediaplayerEnabled[];
#endif
extern const wchar_t kIpcDisabledMessages[];
extern const wchar_t kShowHomeButton[];
extern const wchar_t kShowPageOptionsButtons[];
extern const wchar_t kRecentlySelectedEncoding[];
extern const wchar_t kDeleteBrowsingHistory[];
extern const wchar_t kDeleteDownloadHistory[];
extern const wchar_t kDeleteCache[];
extern const wchar_t kDeleteCookies[];
extern const wchar_t kDeletePasswords[];
extern const wchar_t kDeleteFormData[];
extern const wchar_t kEnableSpellCheck[];
extern const wchar_t kEnableAutoSpellCorrect[];
extern const wchar_t kDeleteTimePeriod[];
extern const wchar_t kPrintingPageHeaderLeft[];
extern const wchar_t kPrintingPageHeaderCenter[];
extern const wchar_t kPrintingPageHeaderRight[];
extern const wchar_t kPrintingPageFooterLeft[];
extern const wchar_t kPrintingPageFooterCenter[];
extern const wchar_t kPrintingPageFooterRight[];
#if defined(TOOLKIT_USES_GTK)
extern const wchar_t kUsesSystemTheme[];
#endif
extern const wchar_t kCurrentThemePackFilename[];
extern const wchar_t kCurrentThemeID[];
extern const wchar_t kCurrentThemeImages[];
extern const wchar_t kCurrentThemeColors[];
extern const wchar_t kCurrentThemeTints[];
extern const wchar_t kCurrentThemeDisplayProperties[];
extern const wchar_t kExtensionsUIDeveloperMode[];
extern const wchar_t kExtensionToolbarSize[];
extern const wchar_t kPluginsLastInternalDirectory[];
extern const wchar_t kPluginsPluginsList[];
extern const wchar_t kPluginsPluginsBlacklist[];
extern const wchar_t kPluginsEnabledInternalPDF[];
extern const wchar_t kCheckDefaultBrowser[];
#if defined(OS_MACOSX)
extern const wchar_t kShowUpdatePromotionInfoBar[];
#endif
extern const wchar_t kUseCustomChromeFrame[];
extern const wchar_t kShowOmniboxSearchHint[];
extern const wchar_t kNTPPromoViewsRemaining[];
extern const wchar_t kDesktopNotificationDefaultContentSetting[];
extern const wchar_t kDesktopNotificationAllowedOrigins[];
extern const wchar_t kDesktopNotificationDeniedOrigins[];
extern const wchar_t kDefaultContentSettings[];
extern const wchar_t kPerHostContentSettings[];  // OBSOLETE
extern const wchar_t kContentSettingsVersion[];
extern const wchar_t kContentSettingsPatterns[];
extern const wchar_t kBlockThirdPartyCookies[];
extern const wchar_t kClearSiteDataOnExit[];
extern const wchar_t kPerHostZoomLevels[];
extern const wchar_t kAutoFillEnabled[];
extern const wchar_t kAutoFillAuxiliaryProfilesEnabled[];
extern const wchar_t kAutoFillDialogPlacement[];
extern const wchar_t kAutoFillPositiveUploadRate[];
extern const wchar_t kAutoFillNegativeUploadRate[];

extern const wchar_t kUseVerticalTabs[];
extern const wchar_t kEnableTranslate[];
extern const wchar_t kPinnedTabs[];

// Local state
extern const wchar_t kMetricsClientID[];
extern const wchar_t kMetricsSessionID[];
extern const wchar_t kMetricsClientIDTimestamp[];
extern const wchar_t kMetricsReportingEnabled[];
extern const wchar_t kMetricsInitialLogs[];
extern const wchar_t kMetricsOngoingLogs[];

extern const wchar_t kProfileMetrics[];
extern const wchar_t kProfilePrefix[];

extern const wchar_t kStabilityExitedCleanly[];
extern const wchar_t kStabilityStatsVersion[];
extern const wchar_t kStabilityStatsBuildTime[];
extern const wchar_t kStabilitySessionEndCompleted[];
extern const wchar_t kStabilityLaunchCount[];
extern const wchar_t kStabilityCrashCount[];
extern const wchar_t kStabilityIncompleteSessionEndCount[];
extern const wchar_t kStabilityPageLoadCount[];
extern const wchar_t kStabilityRendererCrashCount[];
extern const wchar_t kStabilityExtensionRendererCrashCount[];
extern const wchar_t kStabilityLaunchTimeSec[];
extern const wchar_t kStabilityLastTimestampSec[];
extern const wchar_t kStabilityRendererHangCount[];
extern const wchar_t kStabilityChildProcessCrashCount[];

extern const wchar_t kStabilityBreakpadRegistrationSuccess[];
extern const wchar_t kStabilityBreakpadRegistrationFail[];
extern const wchar_t kStabilityDebuggerPresent[];
extern const wchar_t kStabilityDebuggerNotPresent[];

extern const wchar_t kStabilityPluginStats[];
extern const wchar_t kStabilityPluginName[];
extern const wchar_t kStabilityPluginLaunches[];
extern const wchar_t kStabilityPluginInstances[];
extern const wchar_t kStabilityPluginCrashes[];

extern const wchar_t kUninstallMetricsPageLoadCount[];
extern const wchar_t kUninstallLaunchCount[];

extern const wchar_t kUninstallMetricsInstallDate[];
extern const wchar_t kUninstallMetricsUptimeSec[];
extern const wchar_t kUninstallLastLaunchTimeSec[];
extern const wchar_t kUninstallLastObservedRunTimeSec[];

extern const wchar_t kBrowserWindowPlacement[];
extern const wchar_t kTaskManagerWindowPlacement[];
extern const wchar_t kPageInfoWindowPlacement[];
extern const wchar_t kKeywordEditorWindowPlacement[];
extern const wchar_t kPreferencesWindowPlacement[];
extern const wchar_t kMemoryCacheSize[];

extern const wchar_t kDownloadDefaultDirectory[];
extern const wchar_t kDownloadExtensionsToOpen[];
extern const wchar_t kDownloadDirUpgraded[];

extern const wchar_t kSaveFileDefaultDirectory[];

extern const wchar_t kSelectFileLastDirectory[];

extern const wchar_t kHungPluginDetectFrequency[];
extern const wchar_t kPluginMessageResponseTimeout[];

extern const wchar_t kSpellCheckDictionary[];

extern const wchar_t kExcludedSchemes[];

extern const wchar_t kSafeBrowsingClientKey[];
extern const wchar_t kSafeBrowsingWrappedKey[];

extern const wchar_t kOptionsWindowLastTabIndex[];
extern const wchar_t kContentSettingsWindowLastTabIndex[];
extern const wchar_t kCertificateManagerWindowLastTabIndex[];
extern const wchar_t kShouldShowFirstRunBubble[];
extern const wchar_t kShouldUseOEMFirstRunBubble[];
extern const wchar_t kShouldUseMinimalFirstRunBubble[];
extern const wchar_t kShouldShowWelcomePage[];

extern const wchar_t kLastKnownGoogleURL[];
extern const wchar_t kLastKnownIntranetRedirectOrigin[];

extern const wchar_t kCountryIDAtInstall[];
extern const wchar_t kGeoIDAtInstall[];  // OBSOLETE

extern const wchar_t kShutdownType[];
extern const wchar_t kShutdownNumProcesses[];
extern const wchar_t kShutdownNumProcessesSlow[];

extern const wchar_t kRestartLastSessionOnShutdown[];

extern const wchar_t kNumBookmarksOnBookmarkBar[];
extern const wchar_t kNumFoldersOnBookmarkBar[];
extern const wchar_t kNumBookmarksInOtherBookmarkFolder[];
extern const wchar_t kNumFoldersInOtherBookmarkFolder[];

extern const wchar_t kNumKeywords[];

extern const wchar_t kDisableVideoAndChat[];

extern const wchar_t kDisableExtensions[];
extern const wchar_t kShowExtensionShelf[];
extern const wchar_t kBrowserActionContainerWidth[];

extern const wchar_t kLastExtensionsUpdateCheck[];
extern const wchar_t kNextExtensionsUpdateCheck[];

extern const wchar_t kExtensionBlacklistUpdateVersion[];

extern const wchar_t kNTPMostVisitedURLsBlacklist[];
extern const wchar_t kNTPMostVisitedPinnedURLs[];
extern const wchar_t kNTPTipsCache[];
extern const wchar_t kNTPTipsCacheUpdate[];
extern const wchar_t kNTPTipsServer[];
extern const wchar_t kNTPShownSections[];
extern const wchar_t kNTPPrefVersion[];

extern const wchar_t kDevToolsOpenDocked[];
extern const wchar_t kDevToolsSplitLocation[];

extern const wchar_t kSyncLastSyncedTime[];
extern const wchar_t kSyncHasSetupCompleted[];
extern const wchar_t kKeepEverythingSynced[];
extern const wchar_t kSyncBookmarks[];
extern const wchar_t kSyncPasswords[];
extern const wchar_t kSyncPreferences[];
extern const wchar_t kSyncAutofill[];
extern const wchar_t kSyncThemes[];
extern const wchar_t kSyncTypedUrls[];
extern const wchar_t kSyncExtensions[];
extern const wchar_t kSyncManaged[];

extern const wchar_t kWebAppCreateOnDesktop[];
extern const wchar_t kWebAppCreateInAppsMenu[];
extern const wchar_t kWebAppCreateInQuickLaunchBar[];

extern const wchar_t kGeolocationAccessToken[];
extern const wchar_t kGeolocationDefaultContentSetting[];
extern const wchar_t kGeolocationContentSettings[];

extern const wchar_t kLoginDatabaseMigrated[];

extern const wchar_t kCloudPrintServiceURL[];
extern const wchar_t kCloudPrintProxyId[];
extern const wchar_t kCloudPrintAuthToken[];
extern const wchar_t kCloudPrintXMPPAuthToken[];
extern const wchar_t kCloudPrintEmail[];
extern const wchar_t kCloudPrintPrintSystemSettings[];

extern const wchar_t kNoProxyServer[];
extern const wchar_t kProxyAutoDetect[];
extern const wchar_t kProxyServer[];
extern const wchar_t kProxyPacUrl[];
extern const wchar_t kProxyBypassList[];

extern const wchar_t kRegisteredBackgroundContents[];

}  // namespace prefs

#endif  // CHROME_COMMON_PREF_NAMES_H_
