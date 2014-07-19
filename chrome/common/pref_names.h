// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various preferences, for easier changing.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_

#include <stddef.h>

#include "build/build_config.h"

#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/signin/core/common/signin_pref_names.h"

namespace prefs {

// Profile prefs. Please add Local State prefs below instead.
extern const char kDefaultApps[];
extern const char kDefaultAppsInstalled[];
extern const char kDisableScreenshots[];
extern const char kForceEphemeralProfiles[];
extern const char kHomePageIsNewTabPage[];
extern const char kHomePage[];
extern const char kProfileIconVersion[];
extern const char kRestoreOnStartup[];
extern const char kRestoreOnStartupMigrated[];
extern const char kRestoreStartupURLsMigrationTime[];
extern const char kSessionExitedCleanly[];
extern const char kSessionExitType[];
extern const char kSupervisedUserManualHosts[];
extern const char kSupervisedUserManualURLs[];
extern const char kSupervisedUserCustodianEmail[];
extern const char kSupervisedUserCustodianName[];
extern const char kSupervisedUserSharedSettings[];
extern const char kURLsToRestoreOnStartup[];
extern const char kURLsToRestoreOnStartupOld[];

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
extern const char kWebKitPasswordEchoEnabled[];
#endif
extern const char kSafeBrowsingEnabled[];
extern const char kSafeBrowsingExtendedReportingEnabled[];
extern const char kSafeBrowsingDownloadFeedbackEnabled[];
extern const char kSafeBrowsingReportingEnabled[];
extern const char kSafeBrowsingProceedAnywayDisabled[];
extern const char kSafeBrowsingIncidentReportSent[];
extern const char kIncognitoModeAvailability[];
extern const char kSearchSuggestEnabled[];
#if defined(OS_ANDROID)
extern const char kContextualSearchEnabled[];
#endif
extern const char kConfirmToQuitEnabled[];
extern const char kCookieBehavior[];  // OBSOLETE
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
#if defined(OS_ANDROID) || defined(OS_IOS)
extern const char kLastPolicyCheckTime[];
#endif
extern const char kInstantUIZeroSuggestUrlPrefix[];
extern const char kMultipleProfilePrefMigration[];
extern const char kNetworkPredictionEnabled[];
extern const char kNetworkPredictionOptions[];
extern const char kDefaultAppsInstallState[];
extern const char kHideWebStoreIcon[];
#if defined(OS_CHROMEOS)
extern const char kAudioDevicesMute[];
extern const char kAudioDevicesVolumePercent[];
extern const char kAudioMute[];
extern const char kAudioVolumePercent[];
extern const char kSpringChargerCheck[];
extern const char kTapToClickEnabled[];
extern const char kTapDraggingEnabled[];
extern const char kEnableTouchpadThreeFingerClick[];
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
extern const char kLanguageEnabledExtensionImes[];
extern const char kLanguageRemapCapsLockKeyTo[];
extern const char kLanguageRemapSearchKeyTo[];
extern const char kLanguageRemapControlKeyTo[];
extern const char kLanguageRemapAltKeyTo[];
extern const char kLanguageRemapDiamondKeyTo[];
extern const char kLanguageSendFunctionKeys[];
extern const char kLanguageXkbAutoRepeatEnabled[];
extern const char kLanguageXkbAutoRepeatDelay[];
extern const char kLanguageXkbAutoRepeatInterval[];
extern const char kAccessibilityLargeCursorEnabled[];
extern const char kAccessibilityStickyKeysEnabled[];
extern const char kAccessibilitySpokenFeedbackEnabled[];
extern const char kAccessibilityHighContrastEnabled[];
extern const char kAccessibilityScreenMagnifierEnabled[];
extern const char kAccessibilityScreenMagnifierType[];
extern const char kAccessibilityScreenMagnifierScale[];
extern const char kAccessibilityVirtualKeyboardEnabled[];
extern const char kAccessibilityAutoclickEnabled[];
extern const char kAccessibilityAutoclickDelayMs[];
extern const char kShouldAlwaysShowAccessibilityMenu[];
extern const char kLabsAdvancedFilesystemEnabled[];
extern const char kLabsMediaplayerEnabled[];
extern const char kEnableAutoScreenLock[];
extern const char kShowPlanNotifications[];
extern const char kShow3gPromoNotification[];
extern const char kChromeOSReleaseNotesVersion[];
extern const char kUseSharedProxies[];
extern const char kDisplayPowerState[];
extern const char kDisplayProperties[];
extern const char kSecondaryDisplays[];
extern const char kSessionUserActivitySeen[];
extern const char kSessionStartTime[];
extern const char kSessionLengthLimit[];
extern const char kSessionWaitForInitialUserActivity[];
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
extern const char kPowerAcIdleAction[];
extern const char kPowerBatteryIdleAction[];
extern const char kPowerLidClosedAction[];
extern const char kPowerUseAudioActivity[];
extern const char kPowerUseVideoActivity[];
extern const char kPowerAllowScreenWakeLocks[];
extern const char kPowerPresentationScreenDimDelayFactor[];
extern const char kPowerUserActivityScreenDimDelayFactor[];
extern const char kPowerWaitForInitialUserActivity[];
extern const char kTermsOfServiceURL[];
extern const char kUsedPolicyCertificatesOnce[];
extern const char kAttestationEnabled[];
extern const char kAttestationExtensionWhitelist[];
extern const char kTouchHudProjectionEnabled[];
extern const char kOpenNetworkConfiguration[];
extern const char kRAConsentFirstTime[];
extern const char kMultiProfileNeverShowIntro[];
extern const char kMultiProfileWarningShowDismissed[];
extern const char kMultiProfileUserBehavior[];
extern const char kFirstRunTutorialShown[];
extern const char kSAMLOfflineSigninTimeLimit[];
extern const char kSAMLLastGAIASignInTime[];
extern const char kTimeOnOobe[];
extern const char kCurrentWallpaperAppName[];
extern const char kFileSystemProviderMounted[];
extern const char kTouchVirtualKeyboardEnabled[];
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
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const char kUsesSystemTheme[];
#endif
extern const char kCurrentThemePackFilename[];
extern const char kCurrentThemeID[];
extern const char kCurrentThemeImages[];
extern const char kCurrentThemeColors[];
extern const char kCurrentThemeTints[];
extern const char kCurrentThemeDisplayProperties[];
extern const char kExtensionsUIDeveloperMode[];
extern const char kExtensionsUIDismissedADTPromo[];
extern const char kExtensionCommands[];
extern const char kPluginsLastInternalDirectory[];
extern const char kPluginsPluginsList[];
extern const char kPluginsDisabledPlugins[];
extern const char kPluginsDisabledPluginsExceptions[];
extern const char kPluginsEnabledPlugins[];
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
extern const char kDefaultBrowserSettingEnabled[];
#if defined(OS_MACOSX)
extern const char kShowUpdatePromotionInfoBar[];
#endif
extern const char kUseCustomChromeFrame[];
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
extern const char kAutofillDialogAutofillDefault[];
extern const char kAutofillDialogPayWithoutWallet[];
extern const char kAutofillDialogWalletLocationAcceptance[];
extern const char kAutofillDialogSaveData[];
extern const char kAutofillDialogWalletShippingSameAsBilling[];
extern const char kAutofillGeneratedCardBubbleTimesShown[];
extern const char kAutofillDialogDefaults[];

extern const char kPinnedTabs[];

extern const char kDisable3DAPIs[];
extern const char kEnableDeprecatedWebPlatformFeatures[];
extern const char kEnableHyperlinkAuditing[];
extern const char kEnableReferrers[];
extern const char kEnableDoNotTrack[];

extern const char kImportBookmarks[];
extern const char kImportHistory[];
extern const char kImportHomepage[];
extern const char kImportSearchEngine[];
extern const char kImportSavedPasswords[];

extern const char kProfileAvatarIndex[];
extern const char kProfileName[];
extern const char kSupervisedUserId[];

extern const char kProfileGAIAInfoUpdateTime[];
extern const char kProfileGAIAInfoPictureURL[];

extern const char kProfileAvatarTutorialShown[];
extern const char kProfileUserManagerTutorialShown[];

extern const char kInvertNotificationShown[];

extern const char kPrintingEnabled[];
extern const char kPrintPreviewDisabled[];

extern const char kDefaultSupervisedUserFilteringBehavior[];

extern const char kSupervisedUserCreationAllowed[];
extern const char kSupervisedUsers[];

extern const char kMessageCenterDisabledExtensionIds[];
extern const char kMessageCenterDisabledSystemComponentIds[];
extern const char kWelcomeNotificationDismissed[];
extern const char kWelcomeNotificationDismissedLocal[];
extern const char kWelcomeNotificationPreviouslyPoppedUp[];
extern const char kWelcomeNotificationExpirationTimestamp[];

extern const char kFullscreenAllowed[];

extern const char kLocalDiscoveryNotificationsEnabled[];

extern const char kPreferenceResetTime[];
extern const char kProfileResetPromptMemento[];

extern const char kGCMChannelEnabled[];
extern const char kPushMessagingRegistrationCount[];

extern const char kEasyUnlockEnabled[];
extern const char kEasyUnlockShowTutorial[];
extern const char kEasyUnlockPairing[];

extern const char kZeroSuggestCachedResults[];

extern const char kSuggestionsData[];
extern const char kSuggestionsBlacklist[];

// Local state prefs. Please add Profile prefs above instead.
extern const char kCertRevocationCheckingEnabled[];
extern const char kCertRevocationCheckingRequiredLocalAnchors[];
extern const char kSSLVersionMin[];
extern const char kSSLVersionMax[];
extern const char kCipherSuiteBlacklist[];
extern const char kDisableSSLRecordSplitting[];

extern const char kGLVendorString[];
extern const char kGLRendererString[];
extern const char kGLVersionString[];

extern const char kEulaAccepted[];

extern const char kMetricsReportingEnabled[];

// Android has it's own metric / crash reporting implemented in Android
// Java code so kMetricsReportingEnabled doesn't make sense. We use this
// to inform crashes_ui that we have enabled crash reporting.
#if defined(OS_ANDROID)
extern const char kCrashReportingEnabled[];
#endif

extern const char kVariationsLastFetchTime[];
extern const char kVariationsRestrictParameter[];
extern const char kVariationsSeed[];
extern const char kVariationsSeedDate[];
extern const char kVariationsSeedHash[];
extern const char kVariationsSeedSignature[];

extern const char kDeviceOpenNetworkConfiguration[];

extern const char kProfileLastUsed[];
extern const char kProfilesLastActive[];
extern const char kProfilesNumCreated[];
extern const char kProfileInfoCache[];
extern const char kProfileCreatedByVersion[];

extern const char kStabilityPageLoadCount[];
extern const char kStabilityRendererCrashCount[];
extern const char kStabilityExtensionRendererCrashCount[];
extern const char kStabilityRendererHangCount[];
extern const char kStabilityChildProcessCrashCount[];
extern const char kStabilityOtherUserCrashCount[];
extern const char kStabilityKernelCrashCount[];
extern const char kStabilitySystemUncleanShutdownCount[];
#if defined(OS_ANDROID)
extern const char kStabilityForegroundActivityType[];
extern const char kStabilityLaunchedActivityFlags[];
extern const char kStabilityLaunchedActivityCounts[];
extern const char kStabilityCrashedActivityCounts[];
#endif

extern const char kStabilityPluginStats[];
extern const char kStabilityPluginName[];
extern const char kStabilityPluginLaunches[];
extern const char kStabilityPluginInstances[];
extern const char kStabilityPluginCrashes[];
extern const char kStabilityPluginLoadingErrors[];

extern const char kUninstallMetricsPageLoadCount[];
extern const char kUninstallLastLaunchTimeSec[];
extern const char kUninstallLastObservedRunTimeSec[];

extern const char kBrowserSuppressDefaultBrowserPrompt[];

extern const char kBrowserWindowPlacement[];
extern const char kBrowserWindowPlacementPopup[];
extern const char kTaskManagerWindowPlacement[];
extern const char kKeywordEditorWindowPlacement[];
extern const char kPreferencesWindowPlacement[];
extern const char kMemoryCacheSize[];

extern const char kDownloadDefaultDirectory[];
extern const char kDownloadExtensionsToOpen[];
extern const char kDownloadDirUpgraded[];
#if defined(OS_WIN)
extern const char kOpenPdfDownloadInAdobeReader[];
#endif

extern const char kSaveFileDefaultDirectory[];
extern const char kSaveFileType[];

extern const char kAllowFileSelectionDialogs[];
extern const char kDefaultTasksByMimeType[];
extern const char kDefaultTasksBySuffix[];

extern const char kSelectFileLastDirectory[];

extern const char kHungPluginDetectFrequency[];
extern const char kPluginMessageResponseTimeout[];

extern const char kSpellCheckDictionary[];
extern const char kSpellCheckUseSpellingService[];

extern const char kExcludedSchemes[];

extern const char kSafeBrowsingClientKey[];
extern const char kSafeBrowsingWrappedKey[];

extern const char kOptionsWindowLastTabIndex[];
extern const char kContentSettingsWindowLastTabIndex[];
extern const char kCertificateManagerWindowLastTabIndex[];
extern const char kShowFirstRunBubbleOption[];

extern const char kLastKnownIntranetRedirectOrigin[];

extern const char kShutdownType[];
extern const char kShutdownNumProcesses[];
extern const char kShutdownNumProcessesSlow[];

extern const char kRestartLastSessionOnShutdown[];
extern const char kWasRestarted[];
#if defined(OS_WIN)
extern const char kRelaunchMode[];
#endif

extern const char kDisableVideoAndChat[];

extern const char kDisableExtensions[];
extern const char kDisablePluginFinder[];

extern const char kNtpAppPageNames[];
extern const char kNtpCollapsedCurrentlyOpenTabs[];
extern const char kNtpCollapsedForeignSessions[];
extern const char kNtpCollapsedRecentlyClosedTabs[];
extern const char kNtpCollapsedSnapshotDocument[];
extern const char kNtpCollapsedSyncPromo[];
extern const char kNtpDateResourceServer[];
extern const char kNtpMostVisitedURLsBlacklist[];
extern const char kNtpPromoDesktopSessionFound[];
extern const char kNtpPromoResourceCacheUpdate[];
extern const char kNtpShownBookmarksFolder[];
extern const char kNtpShownPage[];
extern const char kNtpTipsResourceServer[];
extern const char kNtpWebStoreEnabled[];

extern const char kDevToolsAdbKey[];
extern const char kDevToolsDisabled[];
extern const char kDevToolsDiscoverUsbDevicesEnabled[];
extern const char kDevToolsEditedFiles[];
extern const char kDevToolsFileSystemPaths[];
extern const char kDevToolsOpenDocked[];
extern const char kDevToolsPortForwardingEnabled[];
extern const char kDevToolsPortForwardingDefaultSet[];
extern const char kDevToolsPortForwardingConfig[];
#if defined(OS_ANDROID)
extern const char kDevToolsRemoteEnabled[];
#endif

extern const char kGoogleServicesPasswordHash[];

extern const char kInvalidationServiceUseGCMChannel[];

extern const char kSignInPromoStartupCount[];
extern const char kSignInPromoUserSkipped[];
extern const char kSignInPromoShowOnFirstRunAllowed[];
extern const char kSignInPromoShowNTPBubble[];

extern const char kWebAppCreateOnDesktop[];
extern const char kWebAppCreateInAppsMenu[];
extern const char kWebAppCreateInQuickLaunchBar[];

extern const char kGeolocationAccessToken[];
#if defined(OS_ANDROID)
extern const char kGeolocationEnabled[];
#endif

#if defined(ENABLE_GOOGLE_NOW)
extern const char kGoogleGeolocationAccessEnabled[];
#endif

extern const char kDefaultAudioCaptureDevice[];
extern const char kDefaultVideoCaptureDevice[];
extern const char kMediaDeviceIdSalt[];

extern const char kRemoteAccessHostFirewallTraversal[];
extern const char kRemoteAccessHostRequireTwoFactor[];
extern const char kRemoteAccessHostDomain[];
extern const char kRemoteAccessHostTalkGadgetPrefix[];
extern const char kRemoteAccessHostRequireCurtain[];
extern const char kRemoteAccessHostAllowClientPairing[];
extern const char kRemoteAccessHostAllowGnubbyAuth[];
extern const char kRemoteAccessHostAllowRelayedConnection[];
extern const char kRemoteAccessHostUdpPortRange[];

extern const char kPrintPreviewStickySettings[];
extern const char kCloudPrintRoot[];
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
extern const char kCloudPrintPrinters[];
extern const char kCloudPrintSubmitEnabled[];
extern const char kCloudPrintUserSettings[];

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
extern const char kAudioCaptureAllowedUrls[];
extern const char kVideoCaptureAllowed[];
extern const char kVideoCaptureAllowedUrls[];

extern const char kHotwordSearchEnabled[];
extern const char kHotwordAudioLoggingEnabled[];
extern const char kHotwordPreviousLanguage[];

#if defined(OS_ANDROID)
extern const char kProtectedMediaIdentifierEnabled[];
#endif

#if defined(OS_CHROMEOS)
extern const char kDeviceSettingsCache[];
extern const char kHardwareKeyboardLayout[];
extern const char kCarrierDealPromoShown[];
extern const char kShouldAutoEnroll[];
extern const char kAutoEnrollmentPowerLimit[];
extern const char kDeviceActivityTimes[];
extern const char kDeviceLocation[];
extern const char kExternalStorageDisabled[];
extern const char kUsersWallpaperInfo[];
extern const char kAudioOutputAllowed[];
extern const char kOwnerPrimaryMouseButtonRight[];
extern const char kOwnerTapToClickEnabled[];
extern const char kUptimeLimit[];
extern const char kRebootAfterUpdate[];
extern const char kDeviceRobotAnyApiRefreshToken[];
extern const char kDeviceEnrollmentRequisition[];
extern const char kDeviceEnrollmentAutoStart[];
extern const char kDeviceEnrollmentCanExit[];
extern const char kTimesHIDDialogShown[];
extern const char kUsersLRUInputMethod[];
extern const char kEchoCheckedOffers[];
extern const char kCachedMultiProfileUserBehavior[];
extern const char kInitialLocale[];
extern const char kOobeComplete[];
extern const char kOobeScreenPending[];
extern const char kDeviceRegistered[];
extern const char kEnrollmentRecoveryRequired[];
extern const char kUsedPolicyCertificates[];
extern const char kServerBackedDeviceState[];
extern const char kCustomizationDefaultWallpaperURL[];
extern const char kLogoutStartedLast[];
#endif

extern const char kClearPluginLSODataEnabled[];
extern const char kPepperFlashSettingsEnabled[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kMediaCacheSize[];

extern const char kChromeOsReleaseChannel[];

extern const char kPerformanceTracingEnabled[];

extern const char kTabStripStackedLayout[];

extern const char kRegisteredBackgroundContents[];

extern const char kShownAutoLaunchInfobar[];

extern const char kAuthSchemes[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kEnableAuthNegotiatePort[];
extern const char kAuthServerWhitelist[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kGSSAPILibraryName[];
extern const char kAllowCrossOriginAuthPrompt[];

extern const char kBuiltInDnsClientEnabled[];

extern const char kRegisteredProtocolHandlers[];
extern const char kIgnoredProtocolHandlers[];
extern const char kPolicyRegisteredProtocolHandlers[];
extern const char kPolicyIgnoredProtocolHandlers[];
extern const char kCustomHandlersEnabled[];

#if defined(OS_MACOSX)
extern const char kUserRemovedLoginItem[];
extern const char kChromeCreatedLoginItem[];
extern const char kMigratedLoginItemPref[];
extern const char kNotifyWhenAppsKeepChromeAlive[];
#endif

extern const char kBackgroundModeEnabled[];
extern const char kHardwareAccelerationModeEnabled[];
extern const char kHardwareAccelerationModePrevious[];

extern const char kDevicePolicyRefreshRate[];

extern const char kFactoryResetRequested[];
extern const char kRollbackRequested[];

extern const char kMessageCenterShowedFirstRunBalloon[];
extern const char kMessageCenterShowIcon[];
extern const char kMessageCenterForcedOnTaskbar[];

extern const char kAttemptedToEnableAutoupdate[];

extern const char kMediaGalleriesUniqueId[];
extern const char kMediaGalleriesRememberedGalleries[];
extern const char kMediaGalleriesLastScanTime[];

#if defined(USE_ASH)
extern const char kShelfAlignment[];
extern const char kShelfAlignmentLocal[];
extern const char kShelfAutoHideBehavior[];
extern const char kShelfAutoHideBehaviorLocal[];
extern const char kShelfChromeIconIndex[];
extern const char kShelfPreferences[];

extern const char kLogoutDialogDurationMs[];
extern const char kPinnedLauncherApps[];
extern const char kShowLogoutButtonInTray[];
#endif

#if defined(USE_AURA)
extern const char kMaxSeparationForGestureTouchesInPixels[];
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
extern const char kOverscrollMinimumThresholdStartTouchpad[];
extern const char kOverscrollVerticalThresholdStart[];
extern const char kOverscrollHorizontalResistThreshold[];
extern const char kOverscrollVerticalResistThreshold[];
#endif

extern const char kNetworkProfileWarningsLeft[];
extern const char kNetworkProfileLastWarningTime[];

#if defined(OS_CHROMEOS)
extern const char kRLZBrand[];
extern const char kRLZDisabled[];
#endif

#if defined(ENABLE_APP_LIST)
extern const char kAppListProfile[];
extern const char kLastAppListLaunchPing[];
extern const char kAppListLaunchCount[];
extern const char kLastAppListAppLaunchPing[];
extern const char kAppListAppLaunchCount[];
extern const char kAppLauncherHasBeenEnabled[];
extern const char kAppListEnableMethod[];
extern const char kAppListEnableTime[];
extern const char kAppLauncherIsEnabled[];
extern const char kAppLauncherShortcutVersion[];
extern const char kShowAppLauncherPromo[];
extern const char kAppLauncherDriveAppMapping[];
#endif

extern const char kAppLaunchForMetroRestart[];
extern const char kAppLaunchForMetroRestartProfile[];
extern const char kAppShortcutsVersion[];

extern const char kModuleConflictBubbleShown[];

extern const char kDRMSalt[];
extern const char kEnableDRM[];

extern const char kWatchdogExtensionActive[];
// TODO(felt): Remove old string in M34.
extern const char kWatchdogExtensionActiveOld[];

#if defined(OS_ANDROID)
extern const char kPartnerBookmarkMappings[];
#endif

extern const char kQuickCheckEnabled[];

}  // namespace prefs

#endif  // CHROME_COMMON_PREF_NAMES_H_
