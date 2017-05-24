// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various preferences, for easier changing.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_

#include <stddef.h>

#include "build/build_config.h"
#include "chrome/common/features.h"
#include "extensions/features/features.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"
#include "rlz/features/features.h"

namespace prefs {

// Profile prefs. Please add Local State prefs below instead.
#if defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_APP_LIST)
extern const char kArcActiveDirectoryPlayUserId[];
extern const char kArcApps[];
extern const char kArcBackupRestoreEnabled[];
extern const char kArcDataRemoveRequested[];
extern const char kArcEnabled[];
extern const char kArcPolicyComplianceReported[];
extern const char kArcTermsAccepted[];
extern const char kArcLocationServiceEnabled[];
extern const char kArcPackages[];
extern const char kArcSetNotificationsEnabledDeferred[];
extern const char kArcSignedIn[];
extern const char kArcCompatibleFilesystemChosen[];
#endif
extern const char kChildAccountStatusKnown[];
extern const char kDefaultApps[];
extern const char kDisableScreenshots[];
extern const char kForceEphemeralProfiles[];
extern const char kHomePageIsNewTabPage[];
extern const char kHomePage[];
extern const char kImportantSitesDialogHistory[];
#if defined(OS_WIN)
extern const char kLastProfileResetTimestamp[];
#endif
extern const char kNewTabPageLocationOverride[];
extern const char kProfileIconVersion[];
extern const char kRestoreOnStartup[];
extern const char kSessionExitedCleanly[];
extern const char kSessionExitType[];
extern const char kSiteEngagementLastUpdateTime[];
extern const char kSupervisedUserApprovedExtensions[];
extern const char kSupervisedUserCustodianEmail[];
extern const char kSupervisedUserCustodianName[];
extern const char kSupervisedUserCustodianProfileImageURL[];
extern const char kSupervisedUserCustodianProfileURL[];
extern const char kSupervisedUserManualHosts[];
extern const char kSupervisedUserManualURLs[];
extern const char kSupervisedUserSafeSites[];
extern const char kSupervisedUserSecondCustodianEmail[];
extern const char kSupervisedUserSecondCustodianName[];
extern const char kSupervisedUserSecondCustodianProfileImageURL[];
extern const char kSupervisedUserSecondCustodianProfileURL[];
extern const char kSupervisedUserSharedSettings[];
extern const char kSupervisedUserWhitelists[];
extern const char kURLsToRestoreOnStartup[];

#if BUILDFLAG(ENABLE_RLZ)
extern const char kRlzPingDelaySeconds[];
#endif  // BUILDFLAG(ENABLE_RLZ)

// For OS_CHROMEOS we maintain the kApplicationLocale property in both local
// state and the user's profile.  The global property determines the locale of
// the login screen, while the user's profile determines their personal locale
// preference.
extern const char kApplicationLocale[];
#if defined(OS_CHROMEOS)
extern const char kApplicationLocaleBackup[];
extern const char kApplicationLocaleAccepted[];
extern const char kOwnerLocale[];
#endif

extern const char kDefaultCharset[];
extern const char kAcceptLanguages[];
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
#if defined(OS_WIN)
extern const char kWebKitFixedFontFamilyArabic[];
#endif
extern const char kWebKitSerifFontFamilyArabic[];
extern const char kWebKitSansSerifFontFamilyArabic[];
#if defined(OS_WIN)
extern const char kWebKitStandardFontFamilyCyrillic[];
extern const char kWebKitFixedFontFamilyCyrillic[];
extern const char kWebKitSerifFontFamilyCyrillic[];
extern const char kWebKitSansSerifFontFamilyCyrillic[];
extern const char kWebKitStandardFontFamilyGreek[];
extern const char kWebKitFixedFontFamilyGreek[];
extern const char kWebKitSerifFontFamilyGreek[];
extern const char kWebKitSansSerifFontFamilyGreek[];
#endif
extern const char kWebKitStandardFontFamilyJapanese[];
extern const char kWebKitFixedFontFamilyJapanese[];
extern const char kWebKitSerifFontFamilyJapanese[];
extern const char kWebKitSansSerifFontFamilyJapanese[];
extern const char kWebKitStandardFontFamilyKorean[];
extern const char kWebKitFixedFontFamilyKorean[];
extern const char kWebKitSerifFontFamilyKorean[];
extern const char kWebKitSansSerifFontFamilyKorean[];
#if defined(OS_WIN)
extern const char kWebKitCursiveFontFamilyKorean[];
#endif
extern const char kWebKitStandardFontFamilySimplifiedHan[];
extern const char kWebKitFixedFontFamilySimplifiedHan[];
extern const char kWebKitSerifFontFamilySimplifiedHan[];
extern const char kWebKitSansSerifFontFamilySimplifiedHan[];
extern const char kWebKitStandardFontFamilyTraditionalHan[];
extern const char kWebKitFixedFontFamilyTraditionalHan[];
extern const char kWebKitSerifFontFamilyTraditionalHan[];
extern const char kWebKitSansSerifFontFamilyTraditionalHan[];
#if defined(OS_WIN) || defined(OS_MACOSX)
extern const char kWebKitCursiveFontFamilySimplifiedHan[];
extern const char kWebKitCursiveFontFamilyTraditionalHan[];
#endif

extern const char kWebKitDefaultFontSize[];
extern const char kWebKitDefaultFixedFontSize[];
extern const char kWebKitMinimumFontSize[];
extern const char kWebKitMinimumLogicalFontSize[];
extern const char kWebKitJavascriptEnabled[];
extern const char kWebKitWebSecurityEnabled[];
extern const char kWebKitLoadsImagesAutomatically[];
extern const char kWebKitPluginsEnabled[];
extern const char kWebKitEncryptedMediaEnabled[];
extern const char kWebKitDomPasteEnabled[];
extern const char kWebKitTextAreasAreResizable[];
extern const char kWebkitTabsToLinks[];
extern const char kWebKitAllowRunningInsecureContent[];
#if defined(OS_ANDROID)
extern const char kWebKitFontScaleFactor[];
extern const char kWebKitForceEnableZoom[];
extern const char kWebKitPasswordEchoEnabled[];
#endif
extern const char kDataSaverEnabled[];
extern const char kSafeBrowsingEnabled[];
extern const char kSafeBrowsingProceedAnywayDisabled[];
extern const char kSafeBrowsingIncidentsSent[];
extern const char kSafeBrowsingExtendedReportingOptInAllowed[];
extern const char kSSLErrorOverrideAllowed[];
extern const char kIncognitoModeAvailability[];
extern const char kSearchSuggestEnabled[];
#if defined(OS_ANDROID)
extern const char kContextualSearchEnabled[];
#endif  // defined(OS_ANDROID)
#if defined(OS_MACOSX)
extern const char kConfirmToQuitEnabled[];
extern const char kShowFullscreenToolbar[];
#endif
extern const char kPromptForDownload[];
extern const char kAlternateErrorPagesEnabled[];
extern const char kDnsPrefetchingStartupList[];
extern const char kDnsPrefetchingHostReferralList[];
extern const char kQuicAllowed[];
extern const char kHttpServerProperties[];
extern const char kNetworkQualities[];
#if defined(OS_ANDROID)
extern const char kLastPolicyCheckTime[];
#endif
extern const char kInstantUIZeroSuggestUrlPrefix[];
extern const char kNetworkPredictionOptions[];
extern const char kDefaultAppsInstallState[];
extern const char kHideWebStoreIcon[];
#if defined(OS_CHROMEOS)
extern const char kTapToClickEnabled[];
extern const char kTapDraggingEnabled[];
extern const char kEnableTouchpadThreeFingerClick[];
extern const char kNaturalScroll[];
extern const char kPrimaryMouseButtonRight[];
extern const char kMouseSensitivity[];
extern const char kTouchpadSensitivity[];
extern const char kUse24HourClock[];
extern const char kResolveTimezoneByGeolocation[];
// TODO(yusukes): Change "kLanguageABC" to "kABC". The current form is too long
// to remember and confusing. The prefs are actually for input methods and i18n
// keyboards, not UI languages.
extern const char kLanguageCurrentInputMethod[];
extern const char kLanguagePreviousInputMethod[];
extern const char kLanguagePreferredLanguages[];
extern const char kLanguagePreferredLanguagesSyncable[];
extern const char kLanguagePreloadEngines[];
extern const char kLanguagePreloadEnginesSyncable[];
extern const char kLanguageEnabledExtensionImes[];
extern const char kLanguageEnabledExtensionImesSyncable[];
extern const char kLanguageImeMenuActivated[];
extern const char kLanguageShouldMergeInputMethods[];
extern const char kLanguageSendFunctionKeys[];
extern const char kLanguageXkbAutoRepeatEnabled[];
extern const char kLanguageXkbAutoRepeatDelay[];
extern const char kLanguageXkbAutoRepeatInterval[];

// TODO(dmazzoni): move accessibility prefs out of chrome/.
// http://crbug.com/594887
extern const char kAccessibilityLargeCursorEnabled[];
extern const char kAccessibilityLargeCursorDipSize[];
extern const char kAccessibilityStickyKeysEnabled[];
extern const char kAccessibilitySpokenFeedbackEnabled[];
extern const char kAccessibilityHighContrastEnabled[];
extern const char kAccessibilityScreenMagnifierCenterFocus[];
extern const char kAccessibilityScreenMagnifierEnabled[];
extern const char kAccessibilityScreenMagnifierType[];
extern const char kAccessibilityScreenMagnifierScale[];
extern const char kAccessibilityVirtualKeyboardEnabled[];
extern const char kAccessibilityMonoAudioEnabled[];
extern const char kAccessibilityAutoclickEnabled[];
extern const char kAccessibilityAutoclickDelayMs[];
extern const char kAccessibilityCaretHighlightEnabled[];
extern const char kAccessibilityCursorHighlightEnabled[];
extern const char kAccessibilityFocusHighlightEnabled[];
extern const char kAccessibilitySelectToSpeakEnabled[];
extern const char kAccessibilitySwitchAccessEnabled[];
extern const char kShouldAlwaysShowAccessibilityMenu[];

extern const char kLabsAdvancedFilesystemEnabled[];
extern const char kLabsMediaplayerEnabled[];
extern const char kEnableAutoScreenLock[];
extern const char kShow3gPromoNotification[];
extern const char kDataSaverPromptsShown[];
extern const char kChromeOSReleaseNotesVersion[];
extern const char kDisplayPowerState[];
extern const char kDisplayProperties[];
extern const char kSecondaryDisplays[];
extern const char kDisplayRotationLock[];
extern const char kEnableStylusTools[];
extern const char kLaunchPaletteOnEjectEvent[];
extern const char kNoteTakingAppId[];
extern const char kNoteTakingAppEnabledOnLockScreen[];
extern const char kSessionUserActivitySeen[];
extern const char kSessionStartTime[];
extern const char kSessionLengthLimit[];
extern const char kSessionWaitForInitialUserActivity[];
extern const char kLastSessionType[];
extern const char kLastSessionLength[];
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
extern const char kPowerLockScreenDimDelayMs[];
extern const char kPowerLockScreenOffDelayMs[];
extern const char kPowerAcIdleAction[];
extern const char kPowerBatteryIdleAction[];
extern const char kPowerLidClosedAction[];
extern const char kPowerUseAudioActivity[];
extern const char kPowerUseVideoActivity[];
extern const char kPowerAllowScreenWakeLocks[];
extern const char kPowerPresentationScreenDimDelayFactor[];
extern const char kPowerUserActivityScreenDimDelayFactor[];
extern const char kPowerWaitForInitialUserActivity[];
extern const char kPowerForceNonzeroBrightnessForUserActivity[];
extern const char kTermsOfServiceURL[];
extern const char kUsedPolicyCertificatesOnce[];
extern const char kAttestationEnabled[];
extern const char kAttestationExtensionWhitelist[];
extern const char kTouchHudProjectionEnabled[];
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
extern const char kTouchscreenEnabled[];
extern const char kTouchpadEnabled[];
extern const char kWakeOnWifiDarkConnect[];
extern const char kCaptivePortalAuthenticationIgnoresProxy[];
extern const char kForceMaximizeOnFirstRun[];
extern const char kPlatformKeys[];
extern const char kUnifiedDesktopEnabledByDefault[];
extern const char kAllowScreenLock[];
extern const char kHatsLastInteractionTimestamp[];
extern const char kHatsSurveyCycleEndTimestamp[];
extern const char kHatsDeviceIsSelected[];
extern const char kQuickUnlockPinSalt[];
extern const char kQuickUnlockPinSecret[];
extern const char kQuickUnlockFingerprintRecord[];
extern const char kEolStatus[];
extern const char kEolNotificationDismissed[];
extern const char kPinUnlockFeatureNotificationShown[];
extern const char kFingerprintUnlockFeatureNotificationShown[];
extern const char kQuickUnlockModeWhitelist[];
extern const char kQuickUnlockTimeout[];
extern const char kPinUnlockMinimumLength[];
extern const char kPinUnlockMaximumLength[];
extern const char kPinUnlockWeakPinsAllowed[];
extern const char kEnableQuickUnlockFingerprint[];
extern const char kInstantTetheringAllowed[];
extern const char kInstantTetheringEnabled[];
#endif  // defined(OS_CHROMEOS)
extern const char kShowHomeButton[];
extern const char kSpeechRecognitionFilterProfanities[];
extern const char kSavingBrowserHistoryDisabled[];
extern const char kAllowDeletingBrowserHistory[];
#if !defined(OS_ANDROID) && !defined(OS_IOS)
extern const char kMdHistoryMenuPromoShown[];
#endif
extern const char kForceGoogleSafeSearch[];
extern const char kForceYouTubeRestrict[];
extern const char kForceSessionSync[];
extern const char kAllowedDomainsForApps[];
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
extern const char kPluginsAlwaysOpenPdfExternally[];
#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kPluginsShowDetails[];
#endif
extern const char kPluginsAllowOutdated[];
extern const char kPluginsAlwaysAuthorize[];
#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kPluginsMetadata[];
extern const char kPluginsResourceCacheUpdate[];
#endif
extern const char kDefaultBrowserLastDeclined[];
extern const char kResetCheckDefaultBrowser[];
extern const char kDefaultBrowserSettingEnabled[];
#if defined(OS_MACOSX)
extern const char kShowUpdatePromotionInfoBar[];
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const char kUseCustomChromeFrame[];
#endif
extern const char kBackShortcutBubbleShownCount[];
#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kContentSettingsPluginWhitelist[];
#endif
#if !defined(OS_ANDROID)
extern const char kPartitionDefaultZoomLevel[];
extern const char kPartitionPerHostZoomLevels[];

extern const char kPinnedTabs[];
#endif  // !defined(OS_ANDROID)

extern const char kDisable3DAPIs[];
extern const char kEnableDeprecatedWebPlatformFeatures[];
extern const char kEnableHyperlinkAuditing[];
extern const char kEnableReferrers[];
extern const char kEnableDoNotTrack[];

extern const char kImportAutofillFormData[];
extern const char kImportBookmarks[];
extern const char kImportHistory[];
extern const char kImportHomepage[];
extern const char kImportSavedPasswords[];
extern const char kImportSearchEngine[];

extern const char kImportDialogAutofillFormData[];
extern const char kImportDialogBookmarks[];
extern const char kImportDialogHistory[];
extern const char kImportDialogSavedPasswords[];
extern const char kImportDialogSearchEngine[];

extern const char kProfileAvatarIndex[];
extern const char kProfileUsingDefaultName[];
extern const char kProfileName[];
extern const char kProfileUsingDefaultAvatar[];
extern const char kProfileUsingGAIAAvatar[];
extern const char kSupervisedUserId[];

extern const char kProfileGAIAInfoUpdateTime[];
extern const char kProfileGAIAInfoPictureURL[];

extern const char kProfileAvatarTutorialShown[];

extern const char kInvertNotificationShown[];

extern const char kPrintingEnabled[];
extern const char kPrintPreviewDisabled[];
extern const char kPrintPreviewDefaultDestinationSelectionRules[];

#if defined(OS_CHROMEOS)
extern const char kPrintingDevices[];
extern const char kRecommendedNativePrinters[];
#endif  // OS_CHROMEOS

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

#if defined(OS_ANDROID)
extern const char kNotificationsVibrateEnabled[];
#endif

extern const char kPushMessagingAppIdentifierMap[];

extern const char kGCMProductCategoryForSubtypes[];

extern const char kEasyUnlockAllowed[];
extern const char kEasyUnlockEnabled[];
extern const char kEasyUnlockPairing[];
extern const char kEasyUnlockProximityRequired[];

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kToolbarIconSurfacingBubbleAcknowledged[];
extern const char kToolbarIconSurfacingBubbleLastShowTime[];
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
extern const char kWebRTCMultipleRoutesEnabled[];
extern const char kWebRTCNonProxiedUdpEnabled[];
extern const char kWebRTCIPHandlingPolicy[];
extern const char kWebRTCUDPPortRange[];
#endif

#if !defined(OS_ANDROID)
extern const char kHasSeenWelcomePage[];
#endif

#if defined(OS_WIN)
extern const char kHasSeenWin10PromoPage[];
#endif

extern const char kGLVendorString[];
extern const char kGLRendererString[];
extern const char kGLVersionString[];

#if defined(OS_ANDROID)
extern const char kGLExtensionsString[];
extern const char kGpuDriverInfoMaxSamples[];
extern const char kGpuDriverInfoResetNotificationStrategy[];
extern const char kGpuDriverInfoShaderVersion[];
extern const char kGpuDriverInfoBuildFingerPrint[];
#endif

// Deprecated preference for metric / crash reporting on Android. Use
// kMetricsReportingEnabled instead.
#if defined(OS_ANDROID)
extern const char kCrashReportingEnabled[];
#endif  // defined(OS_ANDROID)

extern const char kProfileLastUsed[];
extern const char kProfilesLastActive[];
extern const char kProfilesNumCreated[];
extern const char kProfileInfoCache[];
extern const char kProfileCreatedByVersion[];
extern const char kProfilesDeleted[];

extern const char kStabilityOtherUserCrashCount[];
extern const char kStabilityKernelCrashCount[];
extern const char kStabilitySystemUncleanShutdownCount[];
#if defined(OS_ANDROID)
extern const char kStabilityForegroundActivityType[];
extern const char kStabilityLaunchedActivityFlags[];
extern const char kStabilityLaunchedActivityCounts[];
extern const char kStabilityCrashedActivityCounts[];
#endif  // defined(OS_ANDROID)

extern const char kStabilityPluginStats[];
extern const char kStabilityPluginName[];
extern const char kStabilityPluginLaunches[];
extern const char kStabilityPluginInstances[];
extern const char kStabilityPluginCrashes[];
extern const char kStabilityPluginLoadingErrors[];

extern const char kBrowserSuppressDefaultBrowserPrompt[];

extern const char kBrowserWindowPlacement[];
extern const char kBrowserWindowPlacementPopup[];
extern const char kTaskManagerWindowPlacement[];
extern const char kTaskManagerColumnVisibility[];
extern const char kTaskManagerEndProcessEnabled[];
extern const char kAppWindowPlacement[];

extern const char kDownloadDefaultDirectory[];
extern const char kDownloadExtensionsToOpen[];
extern const char kDownloadDirUpgraded[];
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
extern const char kOpenPdfDownloadInSystemReader[];
#endif

extern const char kSaveFileDefaultDirectory[];
extern const char kSaveFileType[];

extern const char kAllowFileSelectionDialogs[];
extern const char kDefaultTasksByMimeType[];
extern const char kDefaultTasksBySuffix[];

extern const char kSelectFileLastDirectory[];

extern const char kExcludedSchemes[];

extern const char kOptionsWindowLastTabIndex[];
extern const char kShowFirstRunBubbleOption[];

extern const char kLastKnownIntranetRedirectOrigin[];

extern const char kShutdownType[];
extern const char kShutdownNumProcesses[];
extern const char kShutdownNumProcessesSlow[];

extern const char kRestartLastSessionOnShutdown[];
#if !defined(OS_ANDROID)
extern const char kSuppressUnsupportedOSWarning[];
extern const char kWasRestarted[];
#endif

extern const char kDisableExtensions[];
extern const char kDisablePluginFinder[];

extern const char kNtpAppPageNames[];
extern const char kNtpCollapsedForeignSessions[];
#if defined(OS_ANDROID)
extern const char kNtpCollapsedRecentlyClosedTabs[];
extern const char kNtpCollapsedSnapshotDocument[];
extern const char kNtpCollapsedSyncPromo[];
extern const char kContentSuggestionsNotificationsEnabled[];
extern const char kContentSuggestionsConsecutiveIgnoredPrefName[];
extern const char kContentSuggestionsNotificationsSentDay[];
extern const char kContentSuggestionsNotificationsSentCount[];
#endif  // defined(OS_ANDROID)
extern const char kNtpShownPage[];

extern const char kDevToolsAdbKey[];
extern const char kDevToolsDisabled[];
extern const char kDevToolsDiscoverUsbDevicesEnabled[];
extern const char kDevToolsEditedFiles[];
extern const char kDevToolsFileSystemPaths[];
extern const char kDevToolsPortForwardingEnabled[];
extern const char kDevToolsPortForwardingDefaultSet[];
extern const char kDevToolsPortForwardingConfig[];
extern const char kDevToolsPreferences[];
extern const char kDevToolsDiscoverTCPTargetsEnabled[];
extern const char kDevToolsTCPDiscoveryConfig[];
#if defined(OS_ANDROID)
extern const char kDevToolsRemoteEnabled[];
#endif

extern const char kGoogleServicesPasswordHash[];

#if !defined(OS_ANDROID)
extern const char kSignInPromoStartupCount[];
extern const char kSignInPromoUserSkipped[];
extern const char kSignInPromoShowOnFirstRunAllowed[];
extern const char kSignInPromoShowNTPBubble[];
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
extern const char kCrossDevicePromoOptedOut[];
extern const char kCrossDevicePromoShouldBeShown[];
extern const char kCrossDevicePromoObservedSingleAccountCookie[];
extern const char kCrossDevicePromoNextFetchListDevicesTime[];
extern const char kCrossDevicePromoNumDevices[];
extern const char kCrossDevicePromoLastDeviceActiveTime[];
#endif

extern const char kWebAppCreateOnDesktop[];
extern const char kWebAppCreateInAppsMenu[];
extern const char kWebAppCreateInQuickLaunchBar[];

extern const char kGeolocationAccessToken[];

extern const char kGoogleNowLauncherEnabled[];

extern const char kDefaultAudioCaptureDevice[];
extern const char kDefaultVideoCaptureDevice[];
extern const char kMediaDeviceIdSalt[];

extern const char kPrintPreviewStickySettings[];
extern const char kCloudPrintRoot[];
extern const char kCloudPrintProxyEnabled[];
extern const char kCloudPrintProxyId[];
extern const char kCloudPrintAuthToken[];
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

extern const char kMaxConnectionsPerProxy[];

extern const char kAudioCaptureAllowed[];
extern const char kAudioCaptureAllowedUrls[];
extern const char kVideoCaptureAllowed[];
extern const char kVideoCaptureAllowedUrls[];

extern const char kHotwordSearchEnabled[];
extern const char kHotwordAlwaysOnSearchEnabled[];
extern const char kHotwordAlwaysOnNotificationSeen[];
extern const char kHotwordAudioLoggingEnabled[];
extern const char kHotwordPreviousLanguage[];

#if defined(OS_CHROMEOS)
extern const char kDeviceSettingsCache[];
extern const char kHardwareKeyboardLayout[];
extern const char kCarrierDealPromoShown[];
extern const char kShouldAutoEnroll[];
extern const char kAutoEnrollmentPowerLimit[];
extern const char kDeviceActivityTimes[];
extern const char kExternalStorageDisabled[];
extern const char kExternalStorageReadOnly[];
extern const char kOwnerPrimaryMouseButtonRight[];
extern const char kOwnerTapToClickEnabled[];
extern const char kUptimeLimit[];
extern const char kRebootAfterUpdate[];
extern const char kDeviceRobotAnyApiRefreshToken[];
extern const char kDeviceEnrollmentRequisition[];
extern const char kDeviceEnrollmentAutoStart[];
extern const char kDeviceEnrollmentCanExit[];
extern const char kDeviceDMToken[];
extern const char kTimesHIDDialogShown[];
extern const char kUsersLastInputMethod[];
extern const char kEchoCheckedOffers[];
extern const char kCachedMultiProfileUserBehavior[];
extern const char kInitialLocale[];
extern const char kOobeComplete[];
extern const char kOobeScreenPending[];
extern const char kOobeMdMode[];
extern const char kOobeControllerDetected[];
extern const char kOobeTimeOfLastUpdateCheckWithoutUpdate[];
extern const char kCanShowOobeGoodiesPage[];
extern const char kDeviceRegistered[];
extern const char kEnrollmentRecoveryRequired[];
extern const char kUsedPolicyCertificates[];
extern const char kServerBackedDeviceState[];
extern const char kCustomizationDefaultWallpaperURL[];
extern const char kLogoutStartedLast[];
extern const char kConsumerManagementStage[];
extern const char kIsBootstrappingSlave[];
extern const char kReportArcStatusEnabled[];
extern const char kNetworkThrottlingEnabled[];
extern const char kTouchscreenEnabledLocal[];
#endif  // defined(OS_CHROMEOS)

extern const char kClearPluginLSODataEnabled[];
extern const char kPepperFlashSettingsEnabled[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kMediaCacheSize[];

extern const char kChromeOsReleaseChannel[];

extern const char kPerformanceTracingEnabled[];

extern const char kTabStripStackedLayout[];

extern const char kRegisteredBackgroundContents[];

#if defined(OS_WIN)
extern const char kLastWelcomedOSVersion[];
extern const char kWelcomePageOnOSUpgradeEnabled[];
#endif

extern const char kAuthSchemes[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kEnableAuthNegotiatePort[];
extern const char kAuthServerWhitelist[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kGSSAPILibraryName[];
extern const char kAuthAndroidNegotiateAccountType[];
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
extern const char kDebuggingFeaturesRequested[];

#if defined(OS_CHROMEOS)
extern const char kResolveDeviceTimezoneByGeolocation[];
extern const char kSystemTimezoneAutomaticDetectionPolicy[];
#endif  // defined(OS_CHROMEOS)

extern const char kEnableMediaRouter[];
#if !defined(OS_ANDROID)
extern const char kShowCastIconInToolbar[];
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
extern const char kAttemptedToEnableAutoupdate[];

extern const char kMediaGalleriesUniqueId[];
extern const char kMediaGalleriesRememberedGalleries[];
#endif  // !defined(OS_ANDROID)

#if defined(USE_AURA)
extern const char kShelfAlignment[];
extern const char kShelfAlignmentLocal[];
extern const char kShelfAutoHideBehavior[];
extern const char kShelfAutoHideBehaviorLocal[];
extern const char kShelfChromeIconIndex[];
extern const char kShelfPreferences[];

extern const char kLogoutDialogDurationMs[];
extern const char kPinnedLauncherApps[];
extern const char kPolicyPinnedLauncherApps[];
extern const char kShowLogoutButtonInTray[];

extern const char kMaxSeparationForGestureTouchesInPixels[];
extern const char kSemiLongPressTimeInMs[];
extern const char kTabScrubActivationDelayInMs[];
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

#if defined(OS_WIN)
extern const char kNetworkProfileWarningsLeft[];
extern const char kNetworkProfileLastWarningTime[];
#endif

#if defined(OS_CHROMEOS)
extern const char kRLZBrand[];
extern const char kRLZDisabled[];
#endif

#if BUILDFLAG(ENABLE_APP_LIST)
extern const char kAppListProfile[];
extern const char kLastAppListLaunchPing[];
extern const char kAppListLaunchCount[];
extern const char kLastAppListAppLaunchPing[];
extern const char kAppListAppLaunchCount[];
extern const char kAppLauncherHasBeenEnabled[];
extern const char kAppListEnableMethod[];
extern const char kAppListEnableTime[];
extern const char kAppListLocalState[];
extern const char kShowAppLauncherPromo[];
extern const char kAppLauncherDriveAppMapping[];
extern const char kAppLauncherUninstalledDriveApps[];
#endif  // BUILDFLAG(ENABLE_APP_LIST)

#if defined(OS_WIN)
extern const char kAppLaunchForMetroRestart[];
extern const char kAppLaunchForMetroRestartProfile[];
#endif
extern const char kAppShortcutsVersion[];

extern const char kModuleConflictBubbleShown[];

extern const char kDRMSalt[];
extern const char kEnableDRM[];

extern const char kWatchdogExtensionActive[];

#if defined(OS_ANDROID)
extern const char kPartnerBookmarkMappings[];
#endif  // defined(OS_ANDROID)

extern const char kQuickCheckEnabled[];
extern const char kPacHttpsUrlStrippingEnabled[];
extern const char kBrowserGuestModeEnabled[];
extern const char kBrowserAddPersonEnabled[];
extern const char kForceBrowserSignin[];

extern const char kEasyUnlockDeviceId[];
extern const char kEasyUnlockHardlockState[];
extern const char kEasyUnlockLocalStateTpmKeys[];
extern const char kEasyUnlockLocalStateUserPrefs[];

extern const char kRecoveryComponentNeedsElevation[];

extern const char kRegisteredSupervisedUserWhitelists[];

#if BUILDFLAG(ENABLE_BACKGROUND)
extern const char kRestartInBackground[];
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kAnimationPolicy[];
#endif

extern const char kBackgroundTracingLastUpload[];

extern const char kAllowDinosaurEasterEgg[];

#if defined(OS_ANDROID)
extern const char kClickedUpdateMenuItem[];
extern const char kLatestVersionWhenClickedUpdateMenuItem[];
#endif

extern const char kMediaRouterCloudServicesPrefSet[];
extern const char kMediaRouterEnableCloudServices[];
extern const char kMediaRouterFirstRunFlowAcknowledged[];
extern const char kMediaRouterTabMirroringSources[];

extern const char kOriginTrialPublicKey[];
extern const char kOriginTrialDisabledFeatures[];
extern const char kOriginTrialDisabledTokens[];

extern const char kComponentUpdatesEnabled[];

#if defined(OS_ANDROID)
extern const char kLocationSettingsBackoffLevelDSE[];
extern const char kLocationSettingsBackoffLevelDefault[];
extern const char kLocationSettingsNextShowDSE[];
extern const char kLocationSettingsNextShowDefault[];

extern const char kSearchGeolocationDisclosureDismissed[];
extern const char kSearchGeolocationDisclosureShownCount[];
extern const char kSearchGeolocationDisclosureLastShowDate[];
extern const char kSearchGeolocationPreDisclosureMetricsRecorded[];
extern const char kSearchGeolocationPostDisclosureMetricsRecorded[];
#endif

extern const char kDSEGeolocationSetting[];

// Deprecated Google-specific version of the above.
extern const char kGoogleDSEGeolocationSettingDeprecated[];

extern const char kWebShareVisitedTargets[];

#if defined(OS_WIN)
extern const char kIOSPromotionEligible[];
extern const char kIOSPromotionDone[];
extern const char kIOSPromotionSMSEntryPoint[];
extern const char kIOSPromotionShownEntryPoints[];
extern const char kIOSPromotionLastImpression[];
extern const char kIOSPromotionVariationId[];
extern const char kNumberSavePasswordsBubbleIOSPromoShown[];
extern const char kSavePasswordsBubbleIOSPromoDismissed[];
extern const char kNumberBookmarksBubbleIOSPromoShown[];
extern const char kBookmarksBubbleIOSPromoDismissed[];
extern const char kNumberBookmarksFootNoteIOSPromoShown[];
extern const char kBookmarksFootNoteIOSPromoDismissed[];
extern const char kNumberHistoryPageIOSPromoShown[];
extern const char kHistoryPageIOSPromoDismissed[];
#endif

extern const char kSettingsResetPromptPromptWave[];
extern const char kSettingsResetPromptLastTriggeredForDefaultSearch[];
extern const char kSettingsResetPromptLastTriggeredForStartupUrls[];
extern const char kSettingsResetPromptLastTriggeredForHomepage[];

#if defined(OS_ANDROID)
extern const char kClipboardLastModifiedTime[];
#endif

}  // namespace prefs

#endif  // CHROME_COMMON_PREF_NAMES_H_
