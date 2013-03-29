// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various preferences, for easier changing.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_

#include <stddef.h>

#include "build/build_config.h"

#include "components/autofill/common/autofill_pref_names.h"

namespace prefs {

// Profile prefs. Please add Local State prefs below instead.
extern const char kDefaultApps[];
extern const char kDefaultAppsInstalled[];
extern const char kDisableScreenshots[];
extern const char kHomePageIsNewTabPage[];
extern const char kHomePage[];
extern const char kHomePageChanged[];
extern const char kManagedModeLocalPassphrase[];
extern const char kManagedModeLocalSalt[];
extern const char kManagedModeManualHosts[];
extern const char kManagedModeManualURLs[];
extern const char kSessionExitedCleanly[];
extern const char kSessionExitType[];
extern const char kRestoreOnStartup[];
extern const char kURLsToRestoreOnStartup[];
extern const char kRestoreOnStartupMigrated[];

// For OS_CHROMEOS we maintain kApplicationLocale property in both local state
// and user's profile.  Global property determines locale of login screen,
// while user's profile determines his personal locale preference.
extern const char kApplicationLocale[];
#if defined(OS_CHROMEOS)
extern const char kApplicationLocaleBackup[];
extern const char kApplicationLocaleAccepted[];
extern const char kOwnerLocale[];
#endif

// Obselete keys, kept only for migration code to the new keys. See
// http://crbug.com/123812
extern const char kGlobalDefaultCharset[];
extern const char kWebKitGlobalDefaultFontSize[];
extern const char kWebKitGlobalDefaultFixedFontSize[];
extern const char kWebKitGlobalMinimumFontSize[];
extern const char kWebKitGlobalMinimumLogicalFontSize[];
extern const char kWebKitGlobalJavascriptEnabled[];
extern const char kWebKitGlobalJavascriptCanOpenWindowsAutomatically[];
extern const char kWebKitGlobalLoadsImagesAutomatically[];
extern const char kWebKitGlobalPluginsEnabled[];
extern const char kWebKitGlobalStandardFontFamily[];
extern const char kWebKitGlobalFixedFontFamily[];
extern const char kWebKitGlobalSerifFontFamily[];
extern const char kWebKitGlobalSansSerifFontFamily[];
extern const char kWebKitGlobalCursiveFontFamily[];
extern const char kWebKitGlobalFantasyFontFamily[];
extern const char kWebKitOldStandardFontFamily[];
extern const char kWebKitOldFixedFontFamily[];
extern const char kWebKitOldSerifFontFamily[];
extern const char kWebKitOldSansSerifFontFamily[];
extern const char kWebKitOldCursiveFontFamily[];
extern const char kWebKitOldFantasyFontFamily[];

extern const char kDefaultCharset[];
extern const char kAcceptLanguages[];
extern const char kStaticEncodings[];
extern const char kShowBookmarkBar[];
extern const char kShowAppsShortcutInBookmarkBar[];
extern const char kBookmarkEditorExpandedNodes[];
extern const char kWebKitCommonScript[];
extern const char kWebKitStandardFontFamily[];
extern const char kWebKitFixedFontFamily[];
extern const char kWebKitSerifFontFamily[];
extern const char kWebKitSansSerifFontFamily[];
extern const char kWebKitCursiveFontFamily[];
extern const char kWebKitFantasyFontFamily[];
extern const char kWebKitPictographFontFamily[];

// ISO 15924 four-letter script codes that per-script font prefs are supported
// for.
extern const char* const kWebKitScriptsForFontFamilyMaps[];
extern const size_t kWebKitScriptsForFontFamilyMapsLength;

// Per-script font pref prefixes.
extern const char kWebKitStandardFontFamilyMap[];
extern const char kWebKitFixedFontFamilyMap[];
extern const char kWebKitSerifFontFamilyMap[];
extern const char kWebKitSansSerifFontFamilyMap[];
extern const char kWebKitCursiveFontFamilyMap[];
extern const char kWebKitFantasyFontFamilyMap[];
extern const char kWebKitPictographFontFamilyMap[];

// Per-script font prefs that have defaults, for easy reference when registering
// the defaults.
extern const char kWebKitStandardFontFamilyArabic[];
extern const char kWebKitFixedFontFamilyArabic[];
extern const char kWebKitSerifFontFamilyArabic[];
extern const char kWebKitSansSerifFontFamilyArabic[];
extern const char kWebKitStandardFontFamilyCyrillic[];
extern const char kWebKitFixedFontFamilyCyrillic[];
extern const char kWebKitSerifFontFamilyCyrillic[];
extern const char kWebKitSansSerifFontFamilyCyrillic[];
extern const char kWebKitStandardFontFamilyGreek[];
extern const char kWebKitFixedFontFamilyGreek[];
extern const char kWebKitSerifFontFamilyGreek[];
extern const char kWebKitSansSerifFontFamilyGreek[];
extern const char kWebKitStandardFontFamilyJapanese[];
extern const char kWebKitFixedFontFamilyJapanese[];
extern const char kWebKitSerifFontFamilyJapanese[];
extern const char kWebKitSansSerifFontFamilyJapanese[];
extern const char kWebKitStandardFontFamilyKorean[];
extern const char kWebKitFixedFontFamilyKorean[];
extern const char kWebKitSerifFontFamilyKorean[];
extern const char kWebKitSansSerifFontFamilyKorean[];
extern const char kWebKitCursiveFontFamilyKorean[];
extern const char kWebKitStandardFontFamilySimplifiedHan[];
extern const char kWebKitFixedFontFamilySimplifiedHan[];
extern const char kWebKitSerifFontFamilySimplifiedHan[];
extern const char kWebKitSansSerifFontFamilySimplifiedHan[];
extern const char kWebKitStandardFontFamilyTraditionalHan[];
extern const char kWebKitFixedFontFamilyTraditionalHan[];
extern const char kWebKitSerifFontFamilyTraditionalHan[];
extern const char kWebKitSansSerifFontFamilyTraditionalHan[];

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
extern const char kWebKitAllowDisplayingInsecureContent[];
extern const char kWebKitAllowRunningInsecureContent[];
#if defined(OS_ANDROID)
extern const char kWebKitFontScaleFactor[];
extern const char kWebKitForceEnableZoom[];
#endif
extern const char kPasswordManagerEnabled[];
extern const char kPasswordManagerAllowShowPasswords[];
extern const char kAutologinEnabled[];
extern const char kReverseAutologinEnabled[];
extern const char kReverseAutologinRejectedEmailList[];
extern const char kSafeBrowsingEnabled[];
extern const char kSafeBrowsingReportingEnabled[];
extern const char kSafeBrowsingProceedAnywayDisabled[];
extern const char kIncognitoModeAvailability[];
extern const char kSearchSuggestEnabled[];
extern const char kConfirmToQuitEnabled[];
extern const char kCookieBehavior[];  // OBSOLETE
extern const char kSyncedDefaultSearchProviderGUID[];
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
extern const char kDefaultSearchProviderAlternateURLs[];
extern const char kDefaultSearchProviderSearchTermsReplacementKey[];
extern const char kSearchProviderOverrides[];
extern const char kSearchProviderOverridesVersion[];
extern const char kPromptForDownload[];
extern const char kAlternateErrorPagesEnabled[];
extern const char kDnsStartupPrefetchList[];  // OBSOLETE
extern const char kDnsPrefetchingStartupList[];
extern const char kDnsHostReferralList[];  // OBSOLETE
extern const char kDnsPrefetchingHostReferralList[];
extern const char kDisableSpdy[];
extern const char kHttpServerProperties[];
extern const char kSpdyServers[];
extern const char kAlternateProtocolServers[];
extern const char kDisabledSchemes[];
extern const char kUrlBlacklist[];
extern const char kUrlWhitelist[];
extern const char kInstantConfirmDialogShown[];
extern const char kInstantEnabled[];
extern const char kInstantExtendedEnabled[];
extern const char kInstantUIZeroSuggestUrlPrefix[];
extern const char kMultipleProfilePrefMigration[];
extern const char kNetworkPredictionEnabled[];
extern const char kDefaultAppsInstallState[];
extern const char kHideWebStoreIcon[];
#if defined(OS_CHROMEOS)
extern const char kAudioMute[];
extern const char kAudioVolumePercent[];
extern const char kTapToClickEnabled[];
extern const char kTapDraggingEnabled[];
extern const char kEnableTouchpadThreeFingerClick[];
extern const char kEnableTouchpadThreeFingerSwipe[];
extern const char kNaturalScroll[];
extern const char kPrimaryMouseButtonRight[];
extern const char kMouseSensitivity[];
extern const char kTouchpadSensitivity[];
extern const char kUse24HourClock[];
extern const char kDisableDrive[];
extern const char kDisableDriveOverCellular[];
extern const char kDisableDriveHostedFiles[];
// TODO(yusukes): Change "kLanguageABC" to "kABC". The current form is too long
// to remember and confusing. The prefs are actually for input methods and i18n
// keyboards, not UI languages.
extern const char kLanguageCurrentInputMethod[];
extern const char kLanguagePreviousInputMethod[];
extern const char kLanguageHotkeyNextEngineInMenu[];
extern const char kLanguageHotkeyPreviousEngine[];
extern const char kLanguagePreferredLanguages[];
extern const char kLanguagePreloadEngines[];
extern const char kLanguageFilteredExtensionImes[];
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
extern const char kLanguageHangulHanjaBindingKeys[];
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
extern const char kLanguageMozcUseHistorySuggest[];
extern const char kLanguageMozcUseDictionarySuggest[];
extern const char kLanguageMozcSuggestionsSize[];
extern const char kLanguageRemapCapsLockKeyTo[];
extern const char kLanguageRemapSearchKeyTo[];
extern const char kLanguageRemapControlKeyTo[];
extern const char kLanguageRemapAltKeyTo[];
extern const char kLanguageRemapDiamondKeyTo[];
extern const char kLanguageXkbAutoRepeatEnabled[];
extern const char kLanguageXkbAutoRepeatDelay[];
extern const char kLanguageXkbAutoRepeatInterval[];
extern const char kSpokenFeedbackEnabled[];
extern const char kHighContrastEnabled[];
extern const char kScreenMagnifierEnabled[];
extern const char kScreenMagnifierType[];
extern const char kScreenMagnifierScale[];
extern const char kVirtualKeyboardEnabled[];
extern const char kShouldAlwaysShowAccessibilityMenu[];
extern const char kLabsAdvancedFilesystemEnabled[];
extern const char kLabsMediaplayerEnabled[];
extern const char kEnableScreenLock[];
extern const char kShowPlanNotifications[];
extern const char kShow3gPromoNotification[];
extern const char kChromeOSReleaseNotesVersion[];
extern const char kUseSharedProxies[];
extern const char kEnableCrosDRM[];
extern const char kDisplayProperties[];
extern const char kPrimaryDisplayID[];
extern const char kSecondaryDisplayLayout[];
extern const char kSecondaryDisplayOffset[];
extern const char kSecondaryDisplays[];
extern const char kSessionStartTime[];
extern const char kSessionLengthLimit[];
extern const char kPowerAcScreenDimDelayMs[];
extern const char kPowerAcScreenOffDelayMs[];
extern const char kPowerAcScreenLockDelayMs[];
extern const char kPowerAcIdleWarningDelayMs[];
extern const char kPowerAcIdleDelayMs[];
extern const char kPowerBatteryScreenDimDelayMs[];
extern const char kPowerBatteryScreenOffDelayMs[];
extern const char kPowerBatteryScreenLockDelayMs[];
extern const char kPowerBatteryIdleWarningDelayMs[];
extern const char kPowerBatteryIdleDelayMs[];
extern const char kPowerIdleAction[];
extern const char kPowerLidClosedAction[];
extern const char kPowerUseAudioActivity[];
extern const char kPowerUseVideoActivity[];
extern const char kPowerPresentationIdleDelayFactor[];
extern const char kTermsOfServiceURL[];
#endif  // defined(OS_CHROMEOS)
extern const char kIpcDisabledMessages[];
extern const char kShowHomeButton[];
extern const char kRecentlySelectedEncoding[];
extern const char kDeleteBrowsingHistory[];
extern const char kDeleteDownloadHistory[];
extern const char kDeleteCache[];
extern const char kDeleteCookies[];
extern const char kDeletePasswords[];
extern const char kDeleteFormData[];
extern const char kDeleteHostedAppsData[];
extern const char kDeauthorizeContentLicenses[];
extern const char kEnableContinuousSpellcheck[];
extern const char kSpeechRecognitionFilterProfanities[];
extern const char kSpeechRecognitionTrayNotificationShownContexts[];
extern const char kEnabledLabsExperiments[];
extern const char kEnableAutoSpellCorrect[];
extern const char kSavingBrowserHistoryDisabled[];
extern const char kAllowDeletingBrowserHistory[];
extern const char kForceSafeSearch[];
extern const char kDeleteTimePeriod[];
extern const char kLastClearBrowsingDataTime[];
#if defined(TOOLKIT_GTK)
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
extern const char kExtensionCommands[];
extern const char kExtensionsSideloadWipeoutBubbleShown[];
extern const char kPluginsLastInternalDirectory[];
extern const char kPluginsPluginsList[];
extern const char kPluginsDisabledPlugins[];
extern const char kPluginsDisabledPluginsExceptions[];
extern const char kPluginsEnabledPlugins[];
extern const char kPluginsEnabledInternalPDF[];
extern const char kPluginsEnabledNaCl[];
extern const char kPluginsMigratedToPepperFlash[];
extern const char kPluginsRemovedOldComponentPepperFlashSettings[];
extern const char kPluginsShowDetails[];
extern const char kPluginsAllowOutdated[];
extern const char kPluginsAlwaysAuthorize[];
#if defined(ENABLE_PLUGIN_INSTALLATION)
extern const char kPluginsMetadata[];
extern const char kPluginsResourceCacheUpdate[];
#endif
extern const char kCheckDefaultBrowser[];
#if defined(OS_WIN)
extern const char kSuppressSwitchToMetroModeOnSetDefault[];
#endif
extern const char kDefaultBrowserSettingEnabled[];
#if defined(OS_MACOSX)
extern const char kShowUpdatePromotionInfoBar[];
#endif
extern const char kUseCustomChromeFrame[];
extern const char kShowOmniboxSearchHint[];
extern const char kDesktopNotificationPosition[];
extern const char kDefaultContentSettings[];
extern const char kContentSettingsClearOnExitMigrated[];
extern const char kContentSettingsVersion[];
extern const char kContentSettingsPatternPairs[];
extern const char kContentSettingsDefaultWhitelistVersion[];
extern const char kContentSettingsPluginWhitelist[];
extern const char kBlockThirdPartyCookies[];
extern const char kClearSiteDataOnExit[];
extern const char kDefaultZoomLevel[];
extern const char kPerHostZoomLevels[];
extern const char kAutofillDialogPayWithoutWallet[];
extern const char kEditBookmarksEnabled[];

extern const char kEnableTranslate[];
extern const char kPinnedTabs[];

extern const char kDisable3DAPIs[];
extern const char kEnableHyperlinkAuditing[];
extern const char kEnableReferrers[];
extern const char kEnableDoNotTrack[];

extern const char kImportBookmarks[];
extern const char kImportHistory[];
extern const char kImportHomepage[];
extern const char kImportSearchEngine[];
extern const char kImportSavedPasswords[];

extern const char kEnterpriseWebStoreURL[];
extern const char kEnterpriseWebStoreName[];

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
extern const char kLocalProfileId[];
extern const char kPasswordsUseLocalProfileId[];
#endif

extern const char kProfileAvatarIndex[];
extern const char kProfileName[];
extern const char kProfileIsManaged[];

extern const char kInvertNotificationShown[];

extern const char kPrintingEnabled[];
extern const char kPrintPreviewDisabled[];

extern const char kDefaultManagedModeFilteringBehavior[];

extern const char kMessageCenterDisabledExtensionIds[];

// Local state prefs. Please add Profile prefs above instead.
extern const char kCertRevocationCheckingEnabled[];
extern const char kSSLVersionMin[];
extern const char kSSLVersionMax[];
extern const char kCipherSuiteBlacklist[];
extern const char kEnableOriginBoundCerts[];
extern const char kDisableSSLRecordSplitting[];
extern const char kEnableMemoryInfo[];

extern const char kGLVendorString[];
extern const char kGLRendererString[];
extern const char kGLVersionString[];

extern const char kMetricsClientID[];
extern const char kMetricsSessionID[];
extern const char kMetricsLowEntropySource[];
extern const char kMetricsClientIDTimestamp[];
extern const char kMetricsReportingEnabled[];
extern const char kMetricsInitialLogsXml[];
extern const char kMetricsInitialLogsProto[];
extern const char kMetricsOngoingLogsXml[];
extern const char kMetricsOngoingLogsProto[];

extern const char kBookmarkPromptEnabled[];
extern const char kBookmarkPromptImpressionCount[];

extern const char kVariationsLastFetchTime[];
extern const char kVariationsRestrictParameter[];
extern const char kVariationsSeed[];
extern const char kVariationsSeedDate[];

extern const char kProfileLastUsed[];
extern const char kProfilesLastActive[];
extern const char kProfilesNumCreated[];
extern const char kProfileInfoCache[];
extern const char kProfileCreatedByVersion[];

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
extern const char kStabilityPluginLoadingErrors[];

extern const char kInstallDate[];
extern const char kUninstallMetricsPageLoadCount[];
extern const char kUninstallLaunchCount[];
extern const char kUninstallMetricsUptimeSec[];
extern const char kUninstallLastLaunchTimeSec[];
extern const char kUninstallLastObservedRunTimeSec[];

extern const char kBrowserSuppressDefaultBrowserPrompt[];

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

extern const char kAllowFileSelectionDialogs[];
extern const char kDefaultTasksByMimeType[];
extern const char kDefaultTasksBySuffix[];

extern const char kSelectFileLastDirectory[];

extern const char kHungPluginDetectFrequency[];
extern const char kPluginMessageResponseTimeout[];

extern const char kSpellCheckDictionary[];
extern const char kSpellCheckConfirmDialogShown[];
extern const char kSpellCheckUseSpellingService[];

extern const char kExcludedSchemes[];

extern const char kSafeBrowsingClientKey[];
extern const char kSafeBrowsingWrappedKey[];

extern const char kOptionsWindowLastTabIndex[];
extern const char kContentSettingsWindowLastTabIndex[];
extern const char kCertificateManagerWindowLastTabIndex[];
extern const char kShowFirstRunBubbleOption[];

extern const char kLastKnownGoogleURL[];
extern const char kLastPromptedGoogleURL[];
extern const char kLastKnownIntranetRedirectOrigin[];

extern const char kCountryIDAtInstall[];
extern const char kGeoIDAtInstall[];  // OBSOLETE

extern const char kShutdownType[];
extern const char kShutdownNumProcesses[];
extern const char kShutdownNumProcessesSlow[];

extern const char kRestartLastSessionOnShutdown[];
extern const char kWasRestarted[];
#if defined(OS_WIN)
extern const char kRestartSwitchMode[];
extern const char kRestartWithAppList[];
#endif

extern const char kNumKeywords[];

extern const char kDisableVideoAndChat[];

extern const char kDisableExtensions[];
extern const char kDisablePluginFinder[];
extern const char kBrowserActionContainerWidth[];

extern const char kLastExtensionsUpdateCheck[];
extern const char kNextExtensionsUpdateCheck[];

extern const char kExtensionAlertsInitializedPref[];
extern const char kExtensionAllowedInstallSites[];
extern const char kExtensionAllowedTypes[];
extern const char kExtensionBlacklistUpdateVersion[];
extern const char kExtensionInstallAllowList[];
extern const char kExtensionInstallDenyList[];
extern const char kExtensionInstallForceList[];
extern const char kExtensionStorageGarbageCollect[];

extern const char kNtpTipsResourceServer[];

extern const char kNtpCollapsedForeignSessions[];
extern const char kNtpMostVisitedURLsBlacklist[];
extern const char kNtpPromoResourceCacheUpdate[];
extern const char kNtpDateResourceServer[];
extern const char kNtpShownBookmarksFolder[];
extern const char kNtpShownPage[];
extern const char kNtpPromoDesktopSessionFound[];
extern const char kNtpWebStoreEnabled[];
extern const char kNtpAppPageNames[];

extern const char kDevToolsDisabled[];
extern const char kDevToolsDockSide[];
extern const char kDevToolsEditedFiles[];
extern const char kDevToolsFileSystemPaths[];
extern const char kDevToolsHSplitLocation[];
extern const char kDevToolsOpenDocked[];
#if defined(OS_ANDROID)
extern const char kDevToolsRemoteEnabled[];
#endif
extern const char kDevToolsVSplitLocation[];
#if defined(OS_ANDROID) || defined(OS_IOS)
// Used by Chrome Mobile.
extern const char kSpdyProxyAuthEnabled[];
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
extern const char kSigninAllowed[];
extern const char kSyncLastSyncedTime[];
extern const char kSyncHasSetupCompleted[];
extern const char kSyncKeepEverythingSynced[];
extern const char kSyncBookmarks[];
extern const char kSyncPasswords[];
extern const char kSyncPreferences[];
extern const char kSyncPriorityPreferences[];
extern const char kSyncAppNotifications[];
extern const char kSyncAppSettings[];
extern const char kSyncApps[];
extern const char kSyncAutofill[];
extern const char kSyncAutofillProfile[];
extern const char kSyncFaviconImages[];
extern const char kSyncFaviconTracking[];
extern const char kSyncThemes[];
extern const char kSyncTypedUrls[];
extern const char kSyncExtensions[];
extern const char kSyncExtensionSettings[];
extern const char kSyncHistoryDeleteDirectives[];
extern const char kSyncDictionary[];
extern const char kSyncManaged[];
extern const char kSyncSearchEngines[];
extern const char kSyncSessions[];
extern const char kSyncSuppressStart[];
extern const char kSyncSyncedNotifications[];
extern const char kSyncTabs[];
extern const char kGoogleServicesLastUsername[];
extern const char kGoogleServicesUsername[];
extern const char kGoogleServicesUsernamePattern[];
extern const char kSyncUsingSecondaryPassphrase[];
extern const char kSyncEncryptionBootstrapToken[];
extern const char kSyncKeystoreEncryptionBootstrapToken[];
extern const char kSyncAcknowledgedSyncTypes[];
// Deprecated in favor of kInvalidatorMaxInvalidationVersions.
extern const char kSyncMaxInvalidationVersions[];
extern const char kSyncSessionsGUID[];

extern const char kInvalidatorClientId[];
extern const char kInvalidatorInvalidationState[];
extern const char kInvalidatorMaxInvalidationVersions[];

extern const char kSyncPromoStartupCount[];
extern const char kSyncPromoViewCount[];
extern const char kSyncPromoUserSkipped[];
extern const char kSyncPromoShowOnFirstRunAllowed[];
extern const char kSyncPromoShowNTPBubble[];
extern const char kSyncPromoErrorMessage[];

extern const char kProfileGAIAInfoUpdateTime[];
extern const char kProfileGAIAInfoPictureURL[];

extern const char kWebAppCreateOnDesktop[];
extern const char kWebAppCreateInAppsMenu[];
extern const char kWebAppCreateInQuickLaunchBar[];

extern const char kGeolocationAccessToken[];
#if defined(OS_ANDROID)
extern const char kGeolocationEnabled[];
#endif

extern const char kDefaultAudioCaptureDevice[];
extern const char kDefaultVideoCaptureDevice[];

extern const char kRemoteAccessHostFirewallTraversal[];
extern const char kRemoteAccessHostRequireTwoFactor[];
extern const char kRemoteAccessHostDomain[];
extern const char kRemoteAccessHostTalkGadgetPrefix[];
extern const char kRemoteAccessHostRequireCurtain[];

extern const char kPrintPreviewStickySettings[];
extern const char kCloudPrintRoot[];
extern const char kCloudPrintServiceURL[];
extern const char kCloudPrintSigninURL[];
extern const char kCloudPrintDialogWidth[];
extern const char kCloudPrintDialogHeight[];
extern const char kCloudPrintSigninDialogWidth[];
extern const char kCloudPrintSigninDialogHeight[];
extern const char kCloudPrintProxyEnabled[];
extern const char kCloudPrintProxyId[];
extern const char kCloudPrintAuthToken[];
extern const char kCloudPrintXMPPAuthToken[];
extern const char kCloudPrintEmail[];
extern const char kCloudPrintPrintSystemSettings[];
extern const char kCloudPrintEnableJobPoll[];
extern const char kCloudPrintRobotRefreshToken[];
extern const char kCloudPrintRobotEmail[];
extern const char kCloudPrintConnectNewPrinters[];
extern const char kCloudPrintXmppPingEnabled[];
extern const char kCloudPrintXmppPingTimeout[];
extern const char kCloudPrintPrinterBlacklist[];
extern const char kCloudPrintSubmitEnabled[];

#if !defined(OS_ANDROID)
extern const char kChromeToMobileDeviceList[];
#endif

extern const char kProxy[];
extern const char kMaxConnectionsPerProxy[];

extern const char kManagedDefaultCookiesSetting[];
extern const char kManagedDefaultImagesSetting[];
extern const char kManagedDefaultJavaScriptSetting[];
extern const char kManagedDefaultPluginsSetting[];
extern const char kManagedDefaultPopupsSetting[];
extern const char kManagedDefaultGeolocationSetting[];
extern const char kManagedDefaultNotificationsSetting[];
extern const char kManagedDefaultMediaStreamSetting[];

extern const char kManagedCookiesAllowedForUrls[];
extern const char kManagedCookiesBlockedForUrls[];
extern const char kManagedCookiesSessionOnlyForUrls[];
extern const char kManagedImagesAllowedForUrls[];
extern const char kManagedImagesBlockedForUrls[];
extern const char kManagedJavaScriptAllowedForUrls[];
extern const char kManagedJavaScriptBlockedForUrls[];
extern const char kManagedPluginsAllowedForUrls[];
extern const char kManagedPluginsBlockedForUrls[];
extern const char kManagedPopupsAllowedForUrls[];
extern const char kManagedPopupsBlockedForUrls[];
extern const char kManagedNotificationsAllowedForUrls[];
extern const char kManagedNotificationsBlockedForUrls[];
extern const char kManagedAutoSelectCertificateForUrls[];

extern const char kAudioCaptureAllowed[];
extern const char kVideoCaptureAllowed[];

#if defined(OS_CHROMEOS)
extern const char kDeviceSettingsCache[];
extern const char kHardwareKeyboardLayout[];
extern const char kCarrierDealPromoShown[];
extern const char kShouldAutoEnroll[];
extern const char kAutoEnrollmentPowerLimit[];
extern const char kDeviceActivityTimes[];
extern const char kDeviceLocation[];
extern const char kSyncSpareBootstrapToken[];
extern const char kExternalStorageDisabled[];
extern const char kUsersWallpaperInfo[];
extern const char kAudioOutputAllowed[];
extern const char kOwnerPrimaryMouseButtonRight[];
extern const char kOwnerTapToClickEnabled[];
extern const char kUptimeLimit[];
extern const char kRebootAfterUpdate[];
#endif

extern const char kClearPluginLSODataEnabled[];
extern const char kPepperFlashSettingsEnabled[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kMediaCacheSize[];

extern const char kChromeOsReleaseChannel[];

extern const char kTabStripLayoutType[];

extern const char kRegisteredBackgroundContents[];

extern const char kShownAutoLaunchInfobar[];

extern const char kAuthSchemes[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kEnableAuthNegotiatePort[];
extern const char kAuthServerWhitelist[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kGSSAPILibraryName[];
extern const char kSpdyProxyAuthOrigin[];
extern const char kAllowCrossOriginAuthPrompt[];

extern const char kBuiltInDnsClientEnabled[];

extern const char kHttpReceivedContentLength[];
extern const char kHttpOriginalContentLength[];
#if defined(OS_ANDROID) || defined(OS_IOS)
extern const char kDailyHttpOriginalContentLength[];
extern const char kDailyHttpReceivedContentLength[];
extern const char kDailyHttpContentLengthLastUpdateDate[];
#endif

extern const char kRegisteredProtocolHandlers[];
extern const char kIgnoredProtocolHandlers[];
extern const char kCustomHandlersEnabled[];

extern const char kUserCreatedLoginItem[];
extern const char kUserRemovedLoginItem[];
extern const char kBackgroundModeEnabled[];
extern const char kHardwareAccelerationModeEnabled[];

extern const char kDevicePolicyRefreshRate[];
extern const char kUserPolicyRefreshRate[];
extern const char kDisableCloudPolicyOnSignin[];

extern const char kFactoryResetRequested[];

extern const char kRecoveryComponentVersion[];
extern const char kComponentUpdaterState[];

extern const char kMediaGalleriesUniqueId[];
extern const char kMediaGalleriesRememberedGalleries[];

#if defined(USE_ASH)
extern const char kShelfAlignment[];
extern const char kShelfAlignmentLocal[];
extern const char kShelfAutoHideBehavior[];
extern const char kShelfAutoHideBehaviorLocal[];
extern const char kPinnedLauncherApps[];
extern const char kShowLogoutButtonInTray[];
extern const char kShelfPreferences[];

extern const char kWorkspaceCyclerShallowerThanSelectedYOffsets[];
extern const char kWorkspaceCyclerDeeperThanSelectedYOffsets[];
extern const char kWorkspaceCyclerSelectedYOffset[];
extern const char kWorkspaceCyclerSelectedScale[];
extern const char kWorkspaceCyclerMinScale[];
extern const char kWorkspaceCyclerMaxScale[];
extern const char kWorkspaceCyclerMinBrightness[];
extern const char kWorkspaceCyclerBackgroundOpacity[];
extern const char kWorkspaceCyclerDesktopWorkspaceBrightness[];
extern const char kWorkspaceCyclerDistanceToInitiateCycling[];
extern const char kWorkspaceCyclerScrollDistanceToCycleToNextWorkspace[];
extern const char kWorkspaceCyclerCyclerStepAnimationDurationRatio[];
extern const char kWorkspaceCyclerStartCyclerAnimationDuration[];
extern const char kWorkspaceCyclerStopCyclerAnimationDuration[];
#endif

#if defined(USE_AURA)
extern const char kFlingVelocityCap[];
extern const char kLongPressTimeInSeconds[];
extern const char kMaxDistanceBetweenTapsForDoubleTap[];
extern const char kMaxDistanceForTwoFingerTapInPixels[];
extern const char kMaxSecondsBetweenDoubleClick[];
extern const char kMaxSeparationForGestureTouchesInPixels[];
extern const char kMaxSwipeDeviationRatio[];
extern const char kMaxTouchDownDurationInSecondsForClick[];
extern const char kMaxTouchMoveInPixelsForClick[];
extern const char kMinDistanceForPinchScrollInPixels[];
extern const char kMinFlickSpeedSquared[];
extern const char kMinPinchUpdateDistanceInPixels[];
extern const char kMinRailBreakVelocity[];
extern const char kMinScrollDeltaSquared[];
extern const char kMinSwipeSpeed[];
extern const char kMinTouchDownDurationInSecondsForClick[];
extern const char kPointsBufferedForVelocity[];
extern const char kRailBreakProportion[];
extern const char kRailStartProportion[];
extern const char kSemiLongPressTimeInSeconds[];
extern const char kTabScrubActivationDelayInMS[];
extern const char kFlingAccelerationCurveCoefficient0[];
extern const char kFlingAccelerationCurveCoefficient1[];
extern const char kFlingAccelerationCurveCoefficient2[];
extern const char kFlingAccelerationCurveCoefficient3[];
extern const char kFlingCurveTouchpadAlpha[];
extern const char kFlingCurveTouchpadBeta[];
extern const char kFlingCurveTouchpadGamma[];
extern const char kFlingCurveTouchscreenAlpha[];
extern const char kFlingCurveTouchscreenBeta[];
extern const char kFlingCurveTouchscreenGamma[];
extern const char kFlingMaxCancelToDownTimeInMs[];
extern const char kFlingMaxTapGapTimeInMs[];
extern const char kOverscrollHorizontalThresholdComplete[];
extern const char kOverscrollVerticalThresholdComplete[];
extern const char kOverscrollMinimumThresholdStart[];
extern const char kOverscrollHorizontalResistThreshold[];
extern const char kOverscrollVerticalResistThreshold[];
#endif

extern const char kInManagedMode[];

extern const char kNetworkProfileWarningsLeft[];
extern const char kNetworkProfileLastWarningTime[];

extern const char kLastPolicyStatisticsUpdate[];

#if defined(OS_CHROMEOS)
extern const char kRLZBrand[];
extern const char kRLZDisabled[];
#endif

extern const char kAppListProfile[];

}  // namespace prefs

#endif  // CHROME_COMMON_PREF_NAMES_H_
