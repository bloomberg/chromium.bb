// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various preferences, for easier changing.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_
#pragma once

#include "build/build_config.h"

namespace prefs {

// Profile prefs
extern const char kAppsPromoCounter[];
extern const char kDefaultAppsInstalled[];
extern const char kHomePageIsNewTabPage[];
extern const char kHomePage[];
extern const char kSessionExitedCleanly[];
extern const char kRestoreOnStartup[];
extern const char kURLsToRestoreOnStartup[];

// For OS_CHROMEOS we maintain kApplicationLocale property in both local state
// and user's profile.  Global property determines locale of login screen,
// while user's profile determines his personal locale preference.
extern const char kApplicationLocale[];
#if defined(OS_CHROMEOS)
extern const char kApplicationLocaleBackup[];
extern const char kApplicationLocaleAccepted[];
extern const char kOwnerLocale[];
#endif

extern const char kDefaultCharset[];
extern const char kAcceptLanguages[];
extern const char kStaticEncodings[];
extern const char kPopupWhitelistedHosts[];
extern const char kShowBookmarkBar[];
extern const char kWebKitStandardFontIsSerif[];
extern const char kWebKitFixedFontFamily[];
extern const char kWebKitSerifFontFamily[];
extern const char kWebKitSansSerifFontFamily[];
extern const char kWebKitCursiveFontFamily[];
extern const char kWebKitFantasyFontFamily[];
extern const char kWebKitDefaultFontSize[];
extern const char kWebKitDefaultFixedFontSize[];
extern const char kWebKitMinimumFontSize[];
extern const char kWebKitMinimumLogicalFontSize[];
extern const char kWebKitJavascriptEnabled[];
extern const char kWebKitWebSecurityEnabled[];
extern const char kWebKitJavascriptCanOpenWindowsAutomatically[];
extern const char kWebKitLoadsImagesAutomatically[];
extern const char kWebKitPluginsEnabled[];
extern const char kWebKitDomPasteEnabled[];
extern const char kWebKitShrinksStandaloneImagesToFit[];
extern const char kWebKitInspectorSettings[];
extern const char kWebKitUsesUniversalDetector[];
extern const char kWebKitTextAreasAreResizable[];
extern const char kWebKitJavaEnabled[];
extern const char kWebkitTabsToLinks[];
extern const char kPasswordManagerEnabled[];
extern const char kPasswordManagerAllowShowPasswords[];
extern const char kFormAutofillEnabled[];  // OBSOLETE
extern const char kSafeBrowsingEnabled[];
extern const char kSafeBrowsingReportingEnabled[];
extern const char kIncognitoEnabled[];
extern const char kSearchSuggestEnabled[];
extern const char kCookieBehavior[];  // OBSOLETE
extern const char kDefaultSearchProviderEnabled[];
extern const char kDefaultSearchProviderSearchURL[];
extern const char kDefaultSearchProviderSuggestURL[];
extern const char kDefaultSearchProviderInstantURL[];
extern const char kDefaultSearchProviderIconURL[];
extern const char kDefaultSearchProviderEncodings[];
extern const char kDefaultSearchProviderName[];
extern const char kDefaultSearchProviderKeyword[];
extern const char kDefaultSearchProviderID[];
extern const char kDefaultSearchProviderPrepopulateID[];
extern const char kSearchProviderOverrides[];
extern const char kSearchProviderOverridesVersion[];
extern const char kPromptForDownload[];
extern const char kAlternateErrorPagesEnabled[];
extern const char kDnsPrefetchingEnabled[];
extern const char kDnsStartupPrefetchList[];  // OBSOLETE
extern const char kDnsPrefetchingStartupList[];
extern const char kDnsHostReferralList[];  // OBSOLETE
extern const char kDnsPrefetchingHostReferralList[];
extern const char kDisableSpdy[];
extern const char kCookiePromptExpanded[];
extern const char kInstantConfirmDialogShown[];
extern const char kInstantEnabled[];
extern const char kInstantEnabledOnce[];
extern const char kInstantEnabledTime[];
extern const char kInstantPromo[];
extern const char kMultipleProfilePrefMigration[];
#if defined(USE_NSS) || defined(USE_OPENSSL)
extern const char kCertRevocationCheckingEnabled[];
extern const char kSSL3Enabled[];
extern const char kTLS1Enabled[];
#endif
#if defined(OS_CHROMEOS)
extern const char kAudioMute[];
extern const char kAudioVolume[];
extern const char kTapToClickEnabled[];
extern const char kTouchpadSensitivity[];
extern const char kLanguageCurrentInputMethod[];
extern const char kLanguagePreviousInputMethod[];
extern const char kLanguageHotkeyNextEngineInMenu[];
extern const char kLanguageHotkeyPreviousEngine[];
extern const char kLanguagePreferredLanguages[];
extern const char kLanguagePreloadEngines[];
extern const char kLanguageChewingAutoShiftCur[];
extern const char kLanguageChewingAddPhraseDirection[];
extern const char kLanguageChewingEasySymbolInput[];
extern const char kLanguageChewingEscCleanAllBuf[];
extern const char kLanguageChewingForceLowercaseEnglish[];
extern const char kLanguageChewingPlainZhuyin[];
extern const char kLanguageChewingPhraseChoiceRearward[];
extern const char kLanguageChewingSpaceAsSelection[];
extern const char kLanguageChewingMaxChiSymbolLen[];
extern const char kLanguageChewingCandPerPage[];
extern const char kLanguageChewingKeyboardType[];
extern const char kLanguageChewingSelKeys[];
extern const char kLanguageChewingHsuSelKeyType[];
extern const char kLanguageHangulKeyboard[];
extern const char kLanguageHangulHanjaKeys[];
extern const char kLanguagePinyinCorrectPinyin[];
extern const char kLanguagePinyinFuzzyPinyin[];
extern const char kLanguagePinyinLookupTablePageSize[];
extern const char kLanguagePinyinShiftSelectCandidate[];
extern const char kLanguagePinyinMinusEqualPage[];
extern const char kLanguagePinyinCommaPeriodPage[];
extern const char kLanguagePinyinAutoCommit[];
extern const char kLanguagePinyinDoublePinyin[];
extern const char kLanguagePinyinDoublePinyinSchema[];
extern const char kLanguagePinyinInitChinese[];
extern const char kLanguagePinyinInitFull[];
extern const char kLanguagePinyinInitFullPunct[];
extern const char kLanguagePinyinInitSimplifiedChinese[];
extern const char kLanguagePinyinTradCandidate[];
extern const char kLanguageMozcPreeditMethod[];
extern const char kLanguageMozcSessionKeymap[];
extern const char kLanguageMozcPunctuationMethod[];
extern const char kLanguageMozcSymbolMethod[];
extern const char kLanguageMozcSpaceCharacterForm[];
extern const char kLanguageMozcHistoryLearningLevel[];
extern const char kLanguageMozcSelectionShortcut[];
extern const char kLanguageMozcShiftKeyModeSwitch[];
extern const char kLanguageMozcNumpadCharacterForm[];
extern const char kLanguageMozcIncognitoMode[];
extern const char kLanguageMozcUseAutoImeTurnOff[];
extern const char kLanguageMozcUseDateConversion[];
extern const char kLanguageMozcUseSingleKanjiConversion[];
extern const char kLanguageMozcUseSymbolConversion[];
extern const char kLanguageMozcUseNumberConversion[];
extern const char kLanguageMozcUseHistorySuggest[];
extern const char kLanguageMozcUseDictionarySuggest[];
extern const char kLanguageMozcSuggestionsSize[];
extern const char kLanguageXkbRemapSearchKeyTo[];
extern const char kLanguageXkbRemapControlKeyTo[];
extern const char kLanguageXkbRemapAltKeyTo[];
extern const char kLanguageXkbAutoRepeatEnabled[];
extern const char kLanguageXkbAutoRepeatDelay[];
extern const char kLanguageXkbAutoRepeatInterval[];
extern const char kAccessibilityEnabled[];
extern const char kLabsAdvancedFilesystemEnabled[];
extern const char kLabsMediaplayerEnabled[];
extern const char kEnableScreenLock[];
extern const char kShowPlanNotifications[];
#endif
extern const char kIpcDisabledMessages[];
extern const char kShowHomeButton[];
extern const char kShowPageOptionsButtons[];
extern const char kRecentlySelectedEncoding[];
extern const char kDeleteBrowsingHistory[];
extern const char kDeleteDownloadHistory[];
extern const char kDeleteCache[];
extern const char kDeleteCookies[];
extern const char kDeletePasswords[];
extern const char kDeleteFormData[];
extern const char kClearPluginLSODataEnabled[];
extern const char kEnableSpellCheck[];
extern const char kEnabledLabsExperiments[];
extern const char kEnableAutoSpellCorrect[];
extern const char kSavingBrowserHistoryDisabled[];
extern const char kDeleteTimePeriod[];
extern const char kPrintingEnabled[];
extern const char kPrintingPageHeaderLeft[];
extern const char kPrintingPageHeaderCenter[];
extern const char kPrintingPageHeaderRight[];
extern const char kPrintingPageFooterLeft[];
extern const char kPrintingPageFooterCenter[];
extern const char kPrintingPageFooterRight[];
#if defined(TOOLKIT_USES_GTK)
extern const char kUsesSystemTheme[];
#endif
extern const char kCurrentThemePackFilename[];
extern const char kCurrentThemeID[];
extern const char kCurrentThemeImages[];
extern const char kCurrentThemeColors[];
extern const char kCurrentThemeTints[];
extern const char kCurrentThemeDisplayProperties[];
extern const char kExtensionsUIDeveloperMode[];
extern const char kExtensionToolbarSize[];
extern const char kPluginsLastInternalDirectory[];
extern const char kPluginsPluginsList[];
extern const char kPluginsPluginsBlacklist[];
extern const char kPluginsEnabledInternalPDF[];
extern const char kPluginsShowSetReaderDefaultInfobar[];
extern const char kPluginsShowDetails[];
extern const char kCheckDefaultBrowser[];
extern const char kDefaultBrowserSettingEnabled[];
#if defined(OS_MACOSX)
extern const char kShowUpdatePromotionInfoBar[];
#endif
extern const char kUseCustomChromeFrame[];
extern const char kShowOmniboxSearchHint[];
extern const char kDesktopNotificationDefaultContentSetting[];
extern const char kDesktopNotificationAllowedOrigins[];
extern const char kDesktopNotificationDeniedOrigins[];
extern const char kDesktopNotificationPosition[];
extern const char kDefaultContentSettings[];
extern const char kPerHostContentSettings[];  // OBSOLETE
extern const char kContentSettingsVersion[];
extern const char kContentSettingsPatterns[];
extern const char kBlockThirdPartyCookies[];
extern const char kBlockNonsandboxedPlugins[];
extern const char kClearSiteDataOnExit[];
extern const char kDefaultZoomLevel[];
extern const char kPerHostZoomLevels[];
extern const char kAutoFillEnabled[];
extern const char kAutoFillAuxiliaryProfilesEnabled[];
extern const char kAutoFillDialogPlacement[];
extern const char kAutoFillPositiveUploadRate[];
extern const char kAutoFillNegativeUploadRate[];
extern const char kAutoFillPersonalDataManagerFirstRun[];

extern const char kUseVerticalTabs[];
extern const char kEnableTranslate[];
extern const char kPinnedTabs[];
extern const char kPolicyRefreshRate[];

// Local state
extern const char kMetricsClientID[];
extern const char kMetricsSessionID[];
extern const char kMetricsClientIDTimestamp[];
extern const char kMetricsReportingEnabled[];
extern const char kMetricsInitialLogs[];
extern const char kMetricsOngoingLogs[];

extern const char kProfileMetrics[];
extern const char kProfilePrefix[];

extern const char kStabilityExitedCleanly[];
extern const char kStabilityStatsVersion[];
extern const char kStabilityStatsBuildTime[];
extern const char kStabilitySessionEndCompleted[];
extern const char kStabilityLaunchCount[];
extern const char kStabilityCrashCount[];
extern const char kStabilityIncompleteSessionEndCount[];
extern const char kStabilityPageLoadCount[];
extern const char kStabilityRendererCrashCount[];
extern const char kStabilityExtensionRendererCrashCount[];
extern const char kStabilityLaunchTimeSec[];
extern const char kStabilityLastTimestampSec[];
extern const char kStabilityRendererHangCount[];
extern const char kStabilityChildProcessCrashCount[];
extern const char kStabilityOtherUserCrashCount[];
extern const char kStabilityKernelCrashCount[];
extern const char kStabilitySystemUncleanShutdownCount[];

extern const char kStabilityBreakpadRegistrationSuccess[];
extern const char kStabilityBreakpadRegistrationFail[];
extern const char kStabilityDebuggerPresent[];
extern const char kStabilityDebuggerNotPresent[];

extern const char kStabilityPluginStats[];
extern const char kStabilityPluginName[];
extern const char kStabilityPluginLaunches[];
extern const char kStabilityPluginInstances[];
extern const char kStabilityPluginCrashes[];

extern const char kUninstallMetricsPageLoadCount[];
extern const char kUninstallLaunchCount[];

extern const char kUninstallMetricsInstallDate[];
extern const char kUninstallMetricsUptimeSec[];
extern const char kUninstallLastLaunchTimeSec[];
extern const char kUninstallLastObservedRunTimeSec[];

extern const char kBrowserWindowPlacement[];
extern const char kTaskManagerWindowPlacement[];
extern const char kKeywordEditorWindowPlacement[];
extern const char kPreferencesWindowPlacement[];
extern const char kMemoryCacheSize[];

extern const char kDownloadDefaultDirectory[];
extern const char kDownloadExtensionsToOpen[];
extern const char kDownloadDirUpgraded[];

extern const char kSaveFileDefaultDirectory[];
extern const char kSaveFileType[];

extern const char kSelectFileLastDirectory[];

extern const char kHungPluginDetectFrequency[];
extern const char kPluginMessageResponseTimeout[];

extern const char kSpellCheckDictionary[];

extern const char kExcludedSchemes[];

extern const char kSafeBrowsingClientKey[];
extern const char kSafeBrowsingWrappedKey[];

extern const char kOptionsWindowLastTabIndex[];
extern const char kContentSettingsWindowLastTabIndex[];
extern const char kCertificateManagerWindowLastTabIndex[];
extern const char kShouldShowFirstRunBubble[];
extern const char kShouldUseOEMFirstRunBubble[];
extern const char kShouldUseMinimalFirstRunBubble[];
extern const char kShouldShowWelcomePage[];

extern const char kLastKnownGoogleURL[];
extern const char kLastPromptedGoogleURL[];
extern const char kLastKnownIntranetRedirectOrigin[];

extern const char kCountryIDAtInstall[];
extern const char kGeoIDAtInstall[];  // OBSOLETE

extern const char kShutdownType[];
extern const char kShutdownNumProcesses[];
extern const char kShutdownNumProcessesSlow[];

extern const char kRestartLastSessionOnShutdown[];

extern const char kNumBookmarksOnBookmarkBar[];
extern const char kNumFoldersOnBookmarkBar[];
extern const char kNumBookmarksInOtherBookmarkFolder[];
extern const char kNumFoldersInOtherBookmarkFolder[];

extern const char kNumKeywords[];

extern const char kDisableVideoAndChat[];

extern const char kDisableExtensions[];
extern const char kBrowserActionContainerWidth[];

extern const char kLastExtensionsUpdateCheck[];
extern const char kNextExtensionsUpdateCheck[];

extern const char kExtensionInstallAllowList[];
extern const char kExtensionInstallDenyList[];

extern const char kExtensionInstallForceList[];

extern const char kExtensionBlacklistUpdateVersion[];

extern const char kExtensionSidebarWidth[];

extern const char kNTPTipsResourceServer[];

extern const char kNTPMostVisitedURLsBlacklist[];
extern const char kNTPMostVisitedPinnedURLs[];
extern const char kNTPPromoResourceCache[];
extern const char kNTPPromoResourceCacheUpdate[];
extern const char kNTPPromoResourceServer[];
extern const char kNTPDateResourceServer[];
extern const char kNTPShownSections[];
extern const char kNTPPrefVersion[];
extern const char kNTPCustomLogoStart[];
extern const char kNTPCustomLogoEnd[];
extern const char kNTPPromoStart[];
extern const char kNTPPromoEnd[];
extern const char kNTPPromoLine[];
extern const char kNTPPromoClosed[];
extern const char kNTPPromoGroup[];
extern const char kNTPPromoGroupTimeSlice[];
extern const char kNTPPromoBuild[];

extern const char kDevToolsDisabled[];
extern const char kDevToolsOpenDocked[];
extern const char kDevToolsSplitLocation[];
extern const char kSyncSessions[];

extern const char kSyncLastSyncedTime[];
extern const char kSyncHasSetupCompleted[];
extern const char kKeepEverythingSynced[];
extern const char kSyncBookmarks[];
extern const char kSyncPasswords[];
extern const char kSyncPreferences[];
extern const char kSyncApps[];
extern const char kSyncAutofill[];
extern const char kSyncAutofillProfile[];
extern const char kSyncThemes[];
extern const char kSyncTypedUrls[];
extern const char kSyncExtensions[];
extern const char kSyncManaged[];
extern const char kSyncSuppressStart[];
extern const char kGoogleServicesUsername[];
extern const char kSyncCredentialsMigrated[];
extern const char kSyncUsingSecondaryPassphrase[];
extern const char kEncryptionBootstrapToken[];
extern const char kAutofillProfileMigrated[];

extern const char kWebAppCreateOnDesktop[];
extern const char kWebAppCreateInAppsMenu[];
extern const char kWebAppCreateInQuickLaunchBar[];

extern const char kGeolocationAccessToken[];
extern const char kGeolocationDefaultContentSetting[];
extern const char kGeolocationContentSettings[];

extern const char kLoginDatabaseMigrated[];

extern const char kCloudPrintServiceURL[];
extern const char kCloudPrintDialogWidth[];
extern const char kCloudPrintDialogHeight[];
extern const char kCloudPrintProxyEnabled[];
extern const char kCloudPrintProxyId[];
extern const char kCloudPrintAuthToken[];
extern const char kCloudPrintXMPPAuthToken[];
extern const char kCloudPrintEmail[];
extern const char kCloudPrintPrintSystemSettings[];

extern const char kRemotingHasSetupCompleted[];
extern const char kRemotingHostEnabled[];

extern const char kProxy[];

extern const char kManagedDefaultCookiesSetting[];
extern const char kManagedDefaultImagesSetting[];
extern const char kManagedDefaultJavaScriptSetting[];
extern const char kManagedDefaultPluginsSetting[];
extern const char kManagedDefaultPopupsSetting[];

#if defined(OS_CHROMEOS)
extern const char kSignedSettingsTempStorage[];
extern const char kHardwareKeyboardLayout[];
#endif

extern const char kRegisteredBackgroundContents[];

extern const char kAuthSchemes[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kEnableAuthNegotiatePort[];
extern const char kAuthServerWhitelist[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kGSSAPILibraryName[];

extern const char kKnownBackgroundPages[];

extern const char kDisable3DAPIs[];

}  // namespace prefs

#endif  // CHROME_COMMON_PREF_NAMES_H_
