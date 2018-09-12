// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_incognito_whitelist.h"

#include <vector>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/rappor/rappor_pref_names.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/variations/pref_names.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
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
#if !defined(OS_ANDROID)
    kAnimationPolicyAllowed, kAnimationPolicyOnce, kAnimationPolicyNone,
#endif  // !defined(OS_ANDROID)

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

#if defined(OS_ANDROID)
    // Clipboard modification state is updated over all profiles.
    prefs::kClipboardLastModifiedTime,
#endif

    // Default browser bar's status is aggregated between regular and incognito
    // modes.
    prefs::kBrowserSuppressDefaultBrowserPrompt,
    prefs::kDefaultBrowserLastDeclined, prefs::kDefaultBrowserSettingEnabled,
    prefs::kResetCheckDefaultBrowser,

    // Devtools preferences are stored cross profiles as they are not storing
    // user data and just keep debugging environment settings.
    prefs::kDevToolsAdbKey, prefs::kDevToolsAvailability,
    prefs::kDevToolsDiscoverUsbDevicesEnabled, prefs::kDevToolsEditedFiles,
    prefs::kDevToolsFileSystemPaths, prefs::kDevToolsPortForwardingEnabled,
    prefs::kDevToolsPortForwardingDefaultSet,
    prefs::kDevToolsPortForwardingConfig, prefs::kDevToolsPreferences,
    prefs::kDevToolsDiscoverTCPTargetsEnabled,
    prefs::kDevToolsTCPDiscoveryConfig,

    // Google URL prefs don't store user data and just keep track of the URL.
    prefs::kLastKnownGoogleURL, prefs::kLastPromptedGoogleURL,

    // Tab stats metrics are aggregated between regular and incognio mode.
    prefs::kTabStatsTotalTabCountMax, prefs::kTabStatsMaxTabsPerWindow,
    prefs::kTabStatsWindowCountMax, prefs::kTabStatsDailySample,

#if defined(OS_MACOSX)
    prefs::kShowFullscreenToolbar,
#endif

// Toggleing custom frames affects all open windows in the profile, hence
// should be written to the regular profile when changed in incognito mode.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    prefs::kUseCustomChromeFrame,
#endif

    // Rappor preferences are not used in incognito mode, but they are written
    // in startup if they don't exist. So if the startup would be in incognito,
    // they need to be persisted.
    rappor::prefs::kRapporCohortSeed, rappor::prefs::kRapporSecret,

    // Reading list preferences are common between incognito and regular mode.
    reading_list::prefs::kReadingListHasUnseenEntries,

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
    // chrome/common/pref_names.h
    prefs::kEnableHyperlinkAuditing,

    // TODO(https://crbug.com/861722): Check with code owners why this pref is
    // required in tests, if possible, update tests and remove.
    prefs::kEnableReferrers, prefs::kEnableDoNotTrack,
    prefs::kEnableEncryptedMedia,

#if defined(OS_ANDROID)
    prefs::kMigratedToSiteNotificationChannels,
    prefs::kClearedBlockedSiteNotificationChannels,
#endif

    prefs::kPushMessagingAppIdentifierMap,

    prefs::kWebRTCMultipleRoutesEnabled, prefs::kWebRTCNonProxiedUdpEnabled,
    prefs::kWebRTCIPHandlingPolicy, prefs::kWebRTCUDPPortRange,

#if defined(OS_WIN)
    prefs::kHasSeenWin10PromoPage,
#if defined(GOOGLE_CHROME_BUILD)
    prefs::kHasSeenGoogleAppsPromoPage,
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
    prefs::kOpenPdfDownloadInSystemReader,
#endif

    prefs::kTrustedDownloadSources,

    prefs::kDefaultTasksByMimeType, prefs::kDefaultTasksBySuffix,

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

    prefs::kGoogleServicesPasswordHash,

    prefs::kWebAppCreateOnDesktop, prefs::kWebAppCreateInAppsMenu,
    prefs::kWebAppCreateInQuickLaunchBar,

    prefs::kWebAppInstallForceList,

    prefs::kGeolocationAccessToken,

    prefs::kDefaultAudioCaptureDevice, prefs::kDefaultVideoCaptureDevice,
    prefs::kMediaDeviceIdSalt, prefs::kMediaStorageIdSalt,

    prefs::kMaxConnectionsPerProxy,

    prefs::kAudioCaptureAllowed, prefs::kAudioCaptureAllowedUrls,
    prefs::kVideoCaptureAllowed, prefs::kVideoCaptureAllowedUrls,

    prefs::kClearPluginLSODataEnabled, prefs::kPepperFlashSettingsEnabled,
    prefs::kDiskCacheDir, prefs::kDiskCacheSize, prefs::kMediaCacheSize,

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

#if defined(OS_WIN)
    prefs::kNetworkProfileWarningsLeft, prefs::kNetworkProfileLastWarningTime,
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

    prefs::kOriginTrialPublicKey, prefs::kOriginTrialDisabledFeatures,
    prefs::kOriginTrialDisabledTokens,

    prefs::kComponentUpdatesEnabled,

    prefs::kMediaEngagementSchemaVersion,
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
