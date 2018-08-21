// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_incognito_whitelist.h"

#include <vector>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/consent_auditor/pref_names.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/onc/onc_pref_names.h"
#include "components/rappor/rappor_pref_names.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/variations/pref_names.h"
#include "components/web_resource/web_resource_pref_names.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// List of keys that can be changed in the user prefs file by the incognito
// profile.
const char* const kPersistentPrefNames[] = {
#if defined(OS_CHROMEOS)
    // Accessibility preferences should be persisted if they are changed in
    // incognito mode.
    ash::prefs::kAccessibilityLargeCursorEnabled,
    ash::prefs::kAccessibilityLargeCursorDipSize,
    ash::prefs::kAccessibilityStickyKeysEnabled,
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    ash::prefs::kAccessibilityHighContrastEnabled,
    ash::prefs::kAccessibilityScreenMagnifierCenterFocus,
    ash::prefs::kAccessibilityScreenMagnifierEnabled,
    ash::prefs::kAccessibilityScreenMagnifierScale,
    ash::prefs::kAccessibilityVirtualKeyboardEnabled,
    ash::prefs::kAccessibilityMonoAudioEnabled,
    ash::prefs::kAccessibilityAutoclickEnabled,
    ash::prefs::kAccessibilityAutoclickDelayMs,
    ash::prefs::kAccessibilityCaretHighlightEnabled,
    ash::prefs::kAccessibilityCursorHighlightEnabled,
    ash::prefs::kAccessibilityFocusHighlightEnabled,
    ash::prefs::kAccessibilitySelectToSpeakEnabled,
    ash::prefs::kAccessibilitySwitchAccessEnabled,
    ash::prefs::kAccessibilityDictationEnabled,
    ash::prefs::kDockedMagnifierEnabled, ash::prefs::kDockedMagnifierScale,
    ash::prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
    ash::prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kShouldAlwaysShowAccessibilityMenu,
#endif  // defined(OS_CHROMEOS)

    // Bookmark preferences are common between incognito and regular mode.
    bookmarks::prefs::kBookmarkEditorExpandedNodes,
    bookmarks::prefs::kEditBookmarksEnabled,
    bookmarks::prefs::kManagedBookmarks,
    bookmarks::prefs::kManagedBookmarksFolderName,
    bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
    bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
    bookmarks::prefs::kShowBookmarkBar,

    // Metrics preferences are out of profile scope and are merged between
    // incognito and regular modes.
    metrics::prefs::kInstallDate, metrics::prefs::kMetricsClientID,
    metrics::prefs::kMetricsDefaultOptIn, metrics::prefs::kMetricsInitialLogs,
    metrics::prefs::kMetricsLowEntropySource, metrics::prefs::kMetricsMachineId,
    metrics::prefs::kMetricsOngoingLogs, metrics::prefs::kMetricsResetIds,

    metrics::prefs::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabledTimestamp,
    metrics::prefs::kMetricsSessionID, metrics::prefs::kMetricsLastSeenPrefix,
    metrics::prefs::kStabilityBreakpadRegistrationFail,
    metrics::prefs::kStabilityBreakpadRegistrationSuccess,
    metrics::prefs::kStabilityBrowserLastLiveTimeStamp,
    metrics::prefs::kStabilityChildProcessCrashCount,
    metrics::prefs::kStabilityCrashCount,
    metrics::prefs::kStabilityCrashCountWithoutGmsCoreUpdate,
    metrics::prefs::kStabilityDebuggerNotPresent,
    metrics::prefs::kStabilityDebuggerPresent,
    metrics::prefs::kStabilityDeferredCount,
    metrics::prefs::kStabilityDiscardCount,
    metrics::prefs::kStabilityExecutionPhase,
    metrics::prefs::kStabilityExitedCleanly,
    metrics::prefs::kStabilityExtensionRendererCrashCount,
    metrics::prefs::kStabilityExtensionRendererFailedLaunchCount,
    metrics::prefs::kStabilityExtensionRendererLaunchCount,
    metrics::prefs::kStabilityGmsCoreVersion,
    metrics::prefs::kStabilityIncompleteSessionEndCount,
    metrics::prefs::kStabilityLaunchCount,
    metrics::prefs::kStabilityPageLoadCount,
    metrics::prefs::kStabilityRendererCrashCount,
    metrics::prefs::kStabilityRendererFailedLaunchCount,
    metrics::prefs::kStabilityRendererHangCount,
    metrics::prefs::kStabilityRendererLaunchCount,
    metrics::prefs::kStabilitySavedSystemProfile,
    metrics::prefs::kStabilitySavedSystemProfileHash,
    metrics::prefs::kStabilitySessionEndCompleted,
    metrics::prefs::kStabilityStatsBuildTime,
    metrics::prefs::kStabilityStatsVersion,
    metrics::prefs::kStabilitySystemCrashCount,
    metrics::prefs::kStabilityVersionMismatchCount,
    metrics::prefs::kUninstallLaunchCount,
    metrics::prefs::kUninstallMetricsPageLoadCount,
    metrics::prefs::kUninstallMetricsUptimeSec, metrics::prefs::kUkmCellDataUse,
    metrics::prefs::kUmaCellDataUse, metrics::prefs::kUserCellDataUse,

    // Google URL prefs don't store user data and just keep track of the URL.
    prefs::kLastKnownGoogleURL, prefs::kLastPromptedGoogleURL,

    // Tab stats metrics are aggregated between regular and incognio mode.
    prefs::kTabStatsTotalTabCountMax, prefs::kTabStatsMaxTabsPerWindow,
    prefs::kTabStatsWindowCountMax, prefs::kTabStatsDailySample,

    // Rappor preferences are not used in incognito mode, but they are written
    // in startup if they don't exist. So if the startup would be in incognito,
    // they need to be persisted.
    rappor::prefs::kRapporCohortSeed, rappor::prefs::kRapporSecret,

    // Although UKMs are not collected in incognito, theses preferences may be
    // changed by UMA/Sync/Unity consent, and need to be the same between
    // incognito and regular modes.
    ukm::prefs::kUkmClientId, ukm::prefs::kUkmPersistedLogs,
    ukm::prefs::kUkmSessionId,

    // Variations preferences maybe changed from incognito mode and should be
    // kept in sync between incognito and regular modes.
    variations::prefs::kVariationsCompressedSeed,
    variations::prefs::kVariationsCountry,
    variations::prefs::kVariationsCrashStreak,
    variations::prefs::kVariationsFailedToFetchSeedStreak,
    variations::prefs::kVariationsLastFetchTime,
    variations::prefs::kVariationsPermanentConsistencyCountry,
    variations::prefs::kVariationsPermutedEntropyCache,
    variations::prefs::kVariationsRestrictParameter,
    variations::prefs::kVariationsSafeCompressedSeed,
    variations::prefs::kVariationsSafeSeedDate,
    variations::prefs::kVariationsSafeSeedFetchTime,
    variations::prefs::kVariationsSafeSeedLocale,
    variations::prefs::kVariationsSafeSeedPermanentConsistencyCountry,
    variations::prefs::kVariationsSafeSeedSessionConsistencyCountry,
    variations::prefs::kVariationsSafeSeedSignature,
    variations::prefs::kVariationsSeedDate,
    variations::prefs::kVariationsSeedSignature,
};

// TODO(https://crbug.com/861722): Remove this list.
// WARNING: PLEASE DO NOT ADD ANYTHING TO THIS LIST.
// This list is temporarily added for transition of incognito preferences
// storage default, from on disk to in memory. All items in this list will be
// audited, checked with owners, and removed or transfered to
// |kPersistentPrefNames|.
const char* const kTemporaryIncognitoWhitelist[] = {
// chrome/browser/accessibility/animation_policy_prefs.h
#if !defined(OS_ANDROID)
    kAnimationPolicyAllowed, kAnimationPolicyOnce, kAnimationPolicyNone,
#endif  // !defined(OS_ANDROID)

    // chrome/common/pref_names.h
    prefs::kAbusiveExperienceInterventionEnforce,
    prefs::kChildAccountStatusKnown, prefs::kDefaultApps,
    prefs::kSafeBrowsingForTrustedSourcesEnabled, prefs::kDisableScreenshots,
    prefs::kDownloadRestrictions, prefs::kForceEphemeralProfiles,
    prefs::kHomePageIsNewTabPage, prefs::kHomePage,
    prefs::kImportantSitesDialogHistory,
#if defined(OS_WIN)
    prefs::kLastProfileResetTimestamp, prefs::kChromeCleanerResetPending,
#endif
    prefs::kNewTabPageLocationOverride, prefs::kRestoreOnStartup,
    prefs::kSessionExitedCleanly, prefs::kSessionExitType,
    prefs::kObservedSessionTime, prefs::kRecurrentSSLInterstitial,
    prefs::kSiteEngagementLastUpdateTime, prefs::kURLsToRestoreOnStartup,

#if BUILDFLAG(ENABLE_RLZ)
    prefs::kRlzPingDelaySeconds,
#endif  // BUILDFLAG(ENABLE_RLZ)

#if defined(OS_CHROMEOS)
    prefs::kApplicationLocaleBackup, prefs::kApplicationLocaleAccepted,
    prefs::kOwnerLocale, prefs::kAllowedUILocales,
#endif

    prefs::kDataSaverEnabled, prefs::kSSLErrorOverrideAllowed,
    prefs::kIncognitoModeAvailability, prefs::kSearchSuggestEnabled,
#if defined(OS_ANDROID)
    prefs::kContextualSearchEnabled,
#endif  // defined(OS_ANDROID)
#if defined(OS_MACOSX) || defined(OS_WIN) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
    prefs::kConfirmToQuitEnabled,
#endif
#if defined(OS_MACOSX)
    prefs::kShowFullscreenToolbar, prefs::kAllowJavascriptAppleEvents,
#endif
    prefs::kPromptForDownload, prefs::kAlternateErrorPagesEnabled,
    prefs::kDnsPrefetchingStartupList, prefs::kDnsPrefetchingHostReferralList,
    prefs::kQuicAllowed, prefs::kNetworkQualities,
    prefs::kNetworkPredictionOptions, prefs::kDefaultAppsInstallState,
    prefs::kHideWebStoreIcon,
#if defined(OS_CHROMEOS)
    prefs::kEnableTouchpadThreeFingerClick, prefs::kNaturalScroll,
    prefs::kPrimaryMouseButtonRight, prefs::kMouseReverseScroll,
    prefs::kMouseSensitivity, prefs::kTouchpadSensitivity,
    prefs::kUse24HourClock, prefs::kUserTimezone,
    prefs::kResolveTimezoneByGeolocation,
    prefs::kResolveTimezoneByGeolocationMethod,
    prefs::kResolveTimezoneByGeolocationMigratedToMethod,

    prefs::kLabsAdvancedFilesystemEnabled, prefs::kLabsMediaplayerEnabled,
    prefs::kShow3gPromoNotification, prefs::kDataSaverPromptsShown,
    prefs::kChromeOSReleaseNotesVersion, prefs::kNoteTakingAppId,
    prefs::kNoteTakingAppEnabledOnLockScreen,
    prefs::kNoteTakingAppsLockScreenWhitelist,
    prefs::kNoteTakingAppsLockScreenToastShown,
    prefs::kRestoreLastLockScreenNote, prefs::kSessionUserActivitySeen,
    prefs::kSessionStartTime, prefs::kSessionLengthLimit,
    prefs::kSessionWaitForInitialUserActivity, prefs::kLastSessionType,
    prefs::kLastSessionLength, prefs::kTermsOfServiceURL,
    prefs::kAttestationEnabled, prefs::kAttestationExtensionWhitelist,
    prefs::kFirstRunTutorialShown, prefs::kSAMLOfflineSigninTimeLimit,
    prefs::kSAMLLastGAIASignInTime, prefs::kTimeOnOobe,
    prefs::kFileSystemProviderMounted, prefs::kTouchVirtualKeyboardEnabled,
    prefs::kWakeOnWifiDarkConnect,
    prefs::kCaptivePortalAuthenticationIgnoresProxy,
    prefs::kForceMaximizeOnFirstRun, prefs::kPlatformKeys,
    prefs::kUnifiedDesktopEnabledByDefault,
    prefs::kHatsLastInteractionTimestamp, prefs::kHatsSurveyCycleEndTimestamp,
    prefs::kHatsDeviceIsSelected, prefs::kQuickUnlockPinSecret,
    prefs::kQuickUnlockFingerprintRecord, prefs::kEolStatus,
    prefs::kEolNotificationDismissed, prefs::kPinUnlockFeatureNotificationShown,
    prefs::kFingerprintUnlockFeatureNotificationShown,
    prefs::kQuickUnlockModeWhitelist, prefs::kQuickUnlockTimeout,
    prefs::kPinUnlockMinimumLength, prefs::kPinUnlockMaximumLength,
    prefs::kPinUnlockWeakPinsAllowed, prefs::kEnableQuickUnlockFingerprint,
    prefs::kCastReceiverEnabled, prefs::kMinimumAllowedChromeVersion,
    prefs::kShowSyncSettingsOnSessionStart, prefs::kTextToSpeechLangToVoiceName,
    prefs::kTextToSpeechRate, prefs::kTextToSpeechPitch,
    prefs::kTextToSpeechVolume, prefs::kFirstScreenStartTime,
    prefs::kCurrentScreenStartTime, prefs::kScreenTimeMinutesUsed,
    prefs::kUsageTimeLimit, prefs::kScreenTimeLastState,
    prefs::kEnableSyncConsent,
#endif  // defined(OS_CHROMEOS)
    prefs::kShowHomeButton, prefs::kSpeechRecognitionFilterProfanities,
    prefs::kSavingBrowserHistoryDisabled, prefs::kAllowDeletingBrowserHistory,
#if !defined(OS_ANDROID)
    prefs::kMdHistoryMenuPromoShown,
#endif
    prefs::kForceGoogleSafeSearch, prefs::kForceYouTubeRestrict,
    prefs::kForceSessionSync, prefs::kAllowedDomainsForApps,
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    prefs::kUsesSystemTheme,
#endif
    prefs::kCurrentThemePackFilename, prefs::kCurrentThemeID,
    prefs::kCurrentThemeImages, prefs::kCurrentThemeColors,
    prefs::kCurrentThemeTints, prefs::kCurrentThemeDisplayProperties,
    prefs::kPluginsLastInternalDirectory, prefs::kPluginsPluginsList,
    prefs::kPluginsDisabledPlugins, prefs::kPluginsDisabledPluginsExceptions,
    prefs::kPluginsEnabledPlugins, prefs::kPluginsAlwaysOpenPdfExternally,
#if BUILDFLAG(ENABLE_PLUGINS)
    prefs::kPluginsShowDetails,
#endif
    prefs::kPluginsAllowOutdated, prefs::kRunAllFlashInAllowMode,
#if BUILDFLAG(ENABLE_PLUGINS)
    prefs::kPluginsMetadata, prefs::kPluginsResourceCacheUpdate,
#endif
    prefs::kDefaultBrowserLastDeclined, prefs::kResetCheckDefaultBrowser,
    prefs::kDefaultBrowserSettingEnabled,
#if defined(OS_MACOSX)
    prefs::kShowUpdatePromotionInfoBar,
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    prefs::kUseCustomChromeFrame,
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
    prefs::kContentSettingsPluginWhitelist,
#endif
#if !defined(OS_ANDROID)
    prefs::kPartitionDefaultZoomLevel, prefs::kPartitionPerHostZoomLevels,

    prefs::kPinnedTabs,
#endif  // !defined(OS_ANDROID)

    prefs::kDisable3DAPIs, prefs::kEnableDeprecatedWebPlatformFeatures,
    prefs::kEnableHyperlinkAuditing,

    // TODO(https://crbug.com/861722): Check with code owners why this pref is
    // required in tests, if possible, update tests and remove.
    prefs::kEnableReferrers, prefs::kEnableDoNotTrack,
    prefs::kEnableEncryptedMedia,

    prefs::kImportBookmarks, prefs::kImportHistory, prefs::kImportHomepage,
    prefs::kImportSavedPasswords, prefs::kImportSearchEngine,

    prefs::kImportDialogBookmarks, prefs::kImportDialogHistory,
    prefs::kImportDialogSavedPasswords, prefs::kImportDialogSearchEngine,

    prefs::kInvertNotificationShown,

    prefs::kPrintingEnabled, prefs::kPrintPreviewDisabled,
    prefs::kPrintPreviewDefaultDestinationSelectionRules,

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    prefs::kPrintPreviewUseSystemDefaultPrinter,
#endif

#if defined(OS_CHROMEOS)
    prefs::kPrintingDevices, prefs::kRecommendedNativePrinters,
    prefs::kRecommendedNativePrintersFile,
    prefs::kRecommendedNativePrintersAccessMode,
    prefs::kRecommendedNativePrintersBlacklist,
    prefs::kRecommendedNativePrintersWhitelist,
    prefs::kUserNativePrintersAllowed,
#endif  // OS_CHROMEOS

    prefs::kMessageCenterDisabledExtensionIds,
    prefs::kMessageCenterDisabledSystemComponentIds,

    prefs::kFullscreenAllowed,

    prefs::kLocalDiscoveryNotificationsEnabled,

#if defined(OS_ANDROID)
    prefs::kNotificationsVibrateEnabled,
    prefs::kMigratedToSiteNotificationChannels,
    prefs::kClearedBlockedSiteNotificationChannels,
#endif

    prefs::kPushMessagingAppIdentifierMap,

    prefs::kGCMProductCategoryForSubtypes,

    prefs::kWebRTCMultipleRoutesEnabled, prefs::kWebRTCNonProxiedUdpEnabled,
    prefs::kWebRTCIPHandlingPolicy, prefs::kWebRTCUDPPortRange,

#if !defined(OS_ANDROID)
    prefs::kHasSeenWelcomePage,
#endif

#if defined(OS_WIN)
    prefs::kHasSeenWin10PromoPage,
#if defined(GOOGLE_CHROME_BUILD)
    prefs::kHasSeenGoogleAppsPromoPage,
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
    prefs::kCrashReportingEnabled,
#endif  // defined(OS_ANDROID)

    prefs::kStabilityOtherUserCrashCount, prefs::kStabilityKernelCrashCount,
    prefs::kStabilitySystemUncleanShutdownCount,

    prefs::kStabilityPluginStats, prefs::kStabilityPluginName,
    prefs::kStabilityPluginLaunches, prefs::kStabilityPluginInstances,
    prefs::kStabilityPluginCrashes, prefs::kStabilityPluginLoadingErrors,

    prefs::kBrowserSuppressDefaultBrowserPrompt,

    // prefs::kBrowserWindowPlacement, prefs::kBrowserWindowPlacementPopup,
    prefs::kTaskManagerWindowPlacement, prefs::kTaskManagerColumnVisibility,
    prefs::kTaskManagerEndProcessEnabled, prefs::kAppWindowPlacement,

    prefs::kDownloadDefaultDirectory, prefs::kDownloadExtensionsToOpen,
    prefs::kDownloadDirUpgraded,
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
    prefs::kOpenPdfDownloadInSystemReader,
#endif
#if defined(OS_ANDROID)
    prefs::kPromptForDownloadAndroid, prefs::kShowMissingSdCardErrorAndroid,
#endif

    // prefs::kSaveFileDefaultDirectory, prefs::kSaveFileType,
    prefs::kTrustedDownloadSources,

    prefs::kAllowFileSelectionDialogs, prefs::kDefaultTasksByMimeType,
    prefs::kDefaultTasksBySuffix,

    prefs::kSelectFileLastDirectory,

    prefs::kExcludedSchemes,

    prefs::kOptionsWindowLastTabIndex,

    prefs::kLastKnownIntranetRedirectOrigin,

    prefs::kShutdownType, prefs::kShutdownNumProcesses,
    prefs::kShutdownNumProcessesSlow,

    prefs::kRestartLastSessionOnShutdown,
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
    prefs::kPromotionalTabsEnabled,
#endif
    prefs::kSuppressUnsupportedOSWarning, prefs::kWasRestarted,
#endif  // !defined(OS_ANDROID)

    prefs::kDevToolsAdbKey, prefs::kDevToolsAvailability,
    prefs::kDevToolsDiscoverUsbDevicesEnabled, prefs::kDevToolsEditedFiles,
    prefs::kDevToolsFileSystemPaths, prefs::kDevToolsPortForwardingEnabled,
    prefs::kDevToolsPortForwardingDefaultSet,
    prefs::kDevToolsPortForwardingConfig, prefs::kDevToolsPreferences,
    prefs::kDevToolsDiscoverTCPTargetsEnabled,
    prefs::kDevToolsTCPDiscoveryConfig,
#if defined(OS_ANDROID)
    prefs::kDevToolsRemoteEnabled,
#endif

    prefs::kGoogleServicesPasswordHash,

    prefs::kWebAppCreateOnDesktop, prefs::kWebAppCreateInAppsMenu,
    prefs::kWebAppCreateInQuickLaunchBar,

    prefs::kWebAppInstallForceList,

    prefs::kGeolocationAccessToken,

    prefs::kDefaultAudioCaptureDevice, prefs::kDefaultVideoCaptureDevice,
    prefs::kMediaDeviceIdSalt, prefs::kMediaStorageIdSalt,

    prefs::kPrintPreviewStickySettings,

    prefs::kMaxConnectionsPerProxy,

    prefs::kAudioCaptureAllowed, prefs::kAudioCaptureAllowedUrls,
    prefs::kVideoCaptureAllowed, prefs::kVideoCaptureAllowedUrls,

#if defined(OS_CHROMEOS)
    prefs::kDeviceSettingsCache, prefs::kHardwareKeyboardLayout,
    prefs::kCarrierDealPromoShown, prefs::kShouldAutoEnroll,
    prefs::kAutoEnrollmentPowerLimit, prefs::kDeviceActivityTimes,
    prefs::kUserActivityTimes, prefs::kExternalStorageDisabled,
    prefs::kExternalStorageReadOnly, prefs::kOwnerPrimaryMouseButtonRight,
    prefs::kUptimeLimit, prefs::kRebootAfterUpdate,
    prefs::kDeviceRobotAnyApiRefreshToken, prefs::kDeviceEnrollmentRequisition,
    prefs::kDeviceEnrollmentAutoStart, prefs::kDeviceEnrollmentCanExit,
    prefs::kDeviceDMToken, prefs::kTimesHIDDialogShown,
    prefs::kUsersLastInputMethod, prefs::kEchoCheckedOffers,
    prefs::kInitialLocale, prefs::kOobeComplete, prefs::kOobeScreenPending,
    prefs::kOobeControllerDetected, prefs::kCanShowOobeGoodiesPage,
    prefs::kDeviceRegistered, prefs::kEnrollmentRecoveryRequired,
    prefs::kServerBackedDeviceState, prefs::kCustomizationDefaultWallpaperURL,
    prefs::kLogoutStartedLast,
    // prefs::kConsumerManagementStage,
    prefs::kIsBootstrappingSlave, prefs::kNetworkThrottlingEnabled,
    prefs::kPowerMetricsDailySample, prefs::kPowerMetricsIdleScreenDimCount,
    prefs::kPowerMetricsIdleScreenOffCount,
    prefs::kPowerMetricsIdleSuspendCount,
    prefs::kPowerMetricsLidClosedSuspendCount, prefs::kReportingUsers,
    prefs::kRemoveUsersRemoteCommand, prefs::kCameraMediaConsolidated,
#endif  // defined(OS_CHROMEOS)

    prefs::kClearPluginLSODataEnabled, prefs::kPepperFlashSettingsEnabled,
    prefs::kDiskCacheDir, prefs::kDiskCacheSize, prefs::kMediaCacheSize,

    prefs::kChromeOsReleaseChannel,

    prefs::kPerformanceTracingEnabled,

    prefs::kTabStripStackedLayout,

    prefs::kRegisteredBackgroundContents,

#if defined(OS_WIN)
    prefs::kLastWelcomedOSVersion, prefs::kWelcomePageOnOSUpgradeEnabled,
#endif

    prefs::kAuthSchemes, prefs::kDisableAuthNegotiateCnameLookup,
    prefs::kEnableAuthNegotiatePort, prefs::kAuthServerWhitelist,
    prefs::kAuthNegotiateDelegateWhitelist, prefs::kGSSAPILibraryName,
    prefs::kAuthAndroidNegotiateAccountType, prefs::kAllowCrossOriginAuthPrompt,

#if defined(OS_POSIX)
    prefs::kNtlmV2Enabled,
#endif  // defined(OS_POSIX)

    prefs::kCertRevocationCheckingEnabled,
    prefs::kCertRevocationCheckingRequiredLocalAnchors,
    prefs::kCertEnableSha1LocalAnchors,
    prefs::kCertEnableSymantecLegacyInfrastructure, prefs::kSSLVersionMin,
    prefs::kSSLVersionMax, prefs::kTLS13Variant, prefs::kCipherSuiteBlacklist,

    prefs::kBuiltInDnsClientEnabled, prefs::kDnsOverHttpsServers,
    prefs::kDnsOverHttpsServerMethods,

    prefs::kRegisteredProtocolHandlers, prefs::kIgnoredProtocolHandlers,
    prefs::kCustomHandlersEnabled,

#if defined(OS_MACOSX)
    prefs::kUserRemovedLoginItem, prefs::kChromeCreatedLoginItem,
    prefs::kMigratedLoginItemPref, prefs::kNotifyWhenAppsKeepChromeAlive,
#endif

    prefs::kBackgroundModeEnabled, prefs::kHardwareAccelerationModeEnabled,
    prefs::kHardwareAccelerationModePrevious,

    prefs::kFactoryResetRequested, prefs::kFactoryResetTPMFirmwareUpdateMode,
    prefs::kDebuggingFeaturesRequested,

#if defined(OS_CHROMEOS)
    prefs::kResolveDeviceTimezoneByGeolocation,
    prefs::kResolveDeviceTimezoneByGeolocationMethod,
    prefs::kSystemTimezoneAutomaticDetectionPolicy,
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
    prefs::kRelaunchNotification,
#endif  // !defined(OS_CHROMEOS)
    prefs::kRelaunchNotificationPeriod,
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    prefs::kAttemptedToEnableAutoupdate,

    prefs::kMediaGalleriesUniqueId, prefs::kMediaGalleriesRememberedGalleries,
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    prefs::kShelfChromeIconIndex, prefs::kPinnedLauncherApps,
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
    prefs::kNetworkProfileWarningsLeft, prefs::kNetworkProfileLastWarningTime,
#endif

#if defined(OS_CHROMEOS)
    prefs::kRLZBrand, prefs::kRLZDisabled,
#endif

#if BUILDFLAG(ENABLE_APP_LIST)
    prefs::kAppListLocalState,
#endif  // BUILDFLAG(ENABLE_APP_LIST)

#if defined(OS_WIN)
    prefs::kAppLaunchForMetroRestart, prefs::kAppLaunchForMetroRestartProfile,
#endif
    prefs::kAppShortcutsVersion,

    prefs::kModuleConflictBubbleShown,

    prefs::kDRMSalt, prefs::kEnableDRM,

    prefs::kWatchdogExtensionActive,

#if defined(OS_ANDROID)
    prefs::kPartnerBookmarkMappings,
#endif  // defined(OS_ANDROID)

    prefs::kQuickCheckEnabled, prefs::kPacHttpsUrlStrippingEnabled,
    prefs::kBrowserGuestModeEnabled, prefs::kBrowserAddPersonEnabled,

    prefs::kRecoveryComponentNeedsElevation,
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
// prefs::kRestartInBackground,
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    prefs::kAnimationPolicy, prefs::kSecurityKeyPermitAttestation,
#endif

    prefs::kBackgroundTracingLastUpload,

    prefs::kAllowDinosaurEasterEgg,

#if defined(OS_ANDROID)
    prefs::kClickedUpdateMenuItem,
    prefs::kLatestVersionWhenClickedUpdateMenuItem,
#endif

    prefs::kOriginTrialPublicKey, prefs::kOriginTrialDisabledFeatures,
    prefs::kOriginTrialDisabledTokens,

    prefs::kComponentUpdatesEnabled,

#if defined(OS_ANDROID)
    prefs::kLocationSettingsBackoffLevelDSE,
    prefs::kLocationSettingsBackoffLevelDefault,
    prefs::kLocationSettingsNextShowDSE,
    prefs::kLocationSettingsNextShowDefault,

    prefs::kSearchGeolocationDisclosureDismissed,
    prefs::kSearchGeolocationDisclosureShownCount,
    prefs::kSearchGeolocationDisclosureLastShowDate,
    prefs::kSearchGeolocationPreDisclosureMetricsRecorded,
    prefs::kSearchGeolocationPostDisclosureMetricsRecorded,
#endif

    prefs::kDSEGeolocationSettingDeprecated,

    prefs::kDSEPermissionsSettings, prefs::kDSEWasDisabledByPolicy,

    prefs::kWebShareVisitedTargets,

#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
    prefs::kIncompatibleApplications, prefs::kModuleBlacklistCacheMD5Digest,
    prefs::kProblematicPrograms, prefs::kThirdPartyBlockingEnabled,
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
    prefs::kClipboardLastModifiedTime,
#endif

    prefs::kMediaEngagementSchemaVersion,

    prefs::kUnsafelyTreatInsecureOriginAsSecure,

    prefs::kIsolateOrigins, prefs::kSitePerProcess,
    prefs::kWebDriverOverridesIncompatiblePolicies,

#if !defined(OS_ANDROID)
    prefs::kAutoplayAllowed, prefs::kAutoplayWhitelist,
#endif

    // components/consent_auditor/pref_names.h
    consent_auditor::prefs::kLocalConsentsDictionary,

    // components/flags_ui/flags_ui_pref_names.h
    flags_ui::prefs::kEnabledLabsExperiments,

    // components/invalidation/impl/invalidation_prefs.h
    invalidation::prefs::kInvalidatorClientId,
    invalidation::prefs::kInvalidatorInvalidationState,
    invalidation::prefs::kInvalidatorSavedInvalidations,
    invalidation::prefs::kInvalidationServiceUseGCMChannel,

    // components/omnibox/browser/omnibox_pref_names.h
    omnibox::kZeroSuggestCachedResults,

    // components/onc/onc_pref_names.h
    onc::prefs::kDeviceOpenNetworkConfiguration,
    onc::prefs::kOpenNetworkConfiguration,

    // components/reading_list/core/reading_list_pref_names.h
    reading_list::prefs::kReadingListHasUnseenEntries,

    // components/search_engines/search_engines_pref_names.h
    prefs::kSyncedDefaultSearchProviderGUID,
    prefs::kDefaultSearchProviderEnabled, prefs::kSearchProviderOverrides,
    prefs::kSearchProviderOverridesVersion, prefs::kCountryIDAtInstall,

    // components/web_resource/web_resource_pref_names.h
    prefs::kEulaAccepted,

// ui/base/ime/chromeos/extension_ime_util.h
#if defined(OS_CHROMEOS)
    chromeos::extension_ime_util::kBrailleImeExtensionId,
    chromeos::extension_ime_util::kBrailleImeExtensionPath,

    // TODO(https://crbug.com/861722): Check with code owners why this pref is
    // required in tests, if possible, update tests and remove.
    chromeos::extension_ime_util::kBrailleImeEngineId,
#endif  // defined(OS_CHROMEOS)
};

}  // namespace

namespace prefs {

std::vector<const char*> GetIncognitoPersistentPrefsWhitelist() {
  std::vector<const char*> whitelist;
  whitelist.insert(whitelist.end(), kPersistentPrefNames,
                   kPersistentPrefNames + base::size(kPersistentPrefNames));

  // TODO(https://crbug.com/861722): Remove after the list is audited and
  // emptied.
  whitelist.insert(
      whitelist.end(), kTemporaryIncognitoWhitelist,
      kTemporaryIncognitoWhitelist + base::size(kTemporaryIncognitoWhitelist));
  return whitelist;
}

}  // namespace prefs
