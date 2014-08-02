// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_

#include "build/build_config.h"

#include "base/base_switches.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/signin/core/common/signin_switches.h"
#include "content/public/common/content_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in
// media/base/media_switches.cc or ui/gl/gl_switches.cc or one of the
// .cc files corresponding to the *_switches.h files included above
// instead.
// -----------------------------------------------------------------------------

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kAllowCrossOriginAuthPrompt[];
extern const char kAllowFileAccess[];
extern const char kAllowHttpScreenCapture[];
extern const char kAllowNaClCrxFsAPI[];
extern const char kAllowNaClFileHandleAPI[];
extern const char kAllowNaClSocketAPI[];
extern const char kAllowOutdatedPlugins[];
extern const char kAllowRunningInsecureContent[];
extern const char kAlternateProtocolProbabilityThreshold[];
extern const char kAlwaysAuthorizePlugins[];
extern const char kAppId[];
extern const char kApp[];
extern const char kAppListStartPageURL[];
extern const char kAppsCheckoutURL[];
extern const char kAppsGalleryDownloadURL[];
extern const char kAppsGalleryURL[];
extern const char kAppsGalleryUpdateURL[];
extern const char kAppModeAuthCode[];
extern const char kAppModeOAuth2Token[];
extern const char kAuthExtensionPath[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kAuthSchemes[];
extern const char kAuthServerWhitelist[];
extern const char kAutoLaunchAtStartup[];
extern const char kCertificateTransparencyLog[];
extern const char kCheckForUpdateIntervalSec[];
extern const char kCheckCloudPrintConnectorPolicy[];
extern const char kCipherSuiteBlacklist[];
extern const char kCloudPrintFile[];
extern const char kCloudPrintJobTitle[];
extern const char kCloudPrintFileType[];
extern const char kCloudPrintPrintTicket[];
extern const char kCloudPrintSetupProxy[];
extern const char kCrashOnHangThreads[];
extern const char kCreateBrowserOnStartupForTests[];
extern const char kCustomLauncherPage[];
extern const char kDebugEnableFrameToggle[];
extern const char kDebugPackedApps[];
extern const char kDiagnostics[];
extern const char kDiagnosticsFormat[];
extern const char kDiagnosticsRecovery[];
extern const char kDisableAnswersInSuggest[];
extern const char kDisableAsyncDns[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kDisableBackgroundNetworking[];
extern const char kDisableBundledPpapiFlash[];
extern const char kDisableClientSidePhishingDetection[];
extern const char kDisableComponentExtensionsWithBackgroundPages[];
extern const char kDisableComponentUpdate[];
extern const char kDisableDefaultApps[];
extern const char kDisableDeviceDiscovery[];
extern const char kDisableDeviceDiscoveryNotifications[];
extern const char kDisableDnsProbes[];
extern const char kDisableDomainReliability[];
extern const char kDisableExtensionsFileAccessCheck[];
extern const char kDisableExtensionsHttpThrottling[];
extern const char kDisableExtensions[];
extern const char kDisableIPv6[];
extern const char kDisableMinimizeOnSecondLauncherItemClick[];
extern const char kDisableMaterialDesignNTP[];
extern const char kDisableNTPOtherSessionsMenu[];
extern const char kDisableOfflineAutoReload[];
extern const char kDisableOfflineAutoReloadVisibleOnly[];
extern const char kDisableOriginChip[];
extern const char kDisablePasswordManagerReauthentication[];
extern const char kDisablePeopleSearch[];
extern const char kDisablePermissionsBubbles[];
extern const char kDisablePopupBlocking[];
extern const char kDisablePreconnect[];
extern const char kDisablePrerenderLocalPredictor[];
extern const char kDisablePrintPreview[];
extern const char kDisablePromptOnRepost[];
extern const char kDisableQuic[];
extern const char kDisableQuicPacing[];
extern const char kDisableQuicPortSelection[];
extern const char kDisableQuicTimeBasedLossDetection[];
extern const char kDisableSavePasswordBubble[];
extern const char kDisableSearchButtonInOmnibox[];
extern const char kDisableSessionCrashedBubble[];
extern const char kDisableScriptedPrintThrottling[];
extern const char kDisableSpdy31[];
extern const char kDisableSuggestionsService[];
extern const char kDisableSync[];
extern const char kDisableSyncSyncedNotifications[];
extern const char kDisableSyncTypes[];
extern const char kDisableWebResources[];
extern const char kDisableZeroBrowsersOpenForTests[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kDnsLogDetails[];
extern const char kDnsPrefetchDisable[];
extern const char kDumpBrowserHistograms[];
extern const char kEasyUnlockAppPath[];
extern const char kEnableAnswersInSuggest[];
extern const char kEnableAppList[];
extern const char kEnableAppView[];
extern const char kEnableAppWindowControls[];
extern const char kEnableAppsFileAssociations[];
extern const char kEnableAppsShowOnFirstPaint[];
extern const char kEnableAsyncDns[];
extern const char kEnableAuthNegotiatePort[];
extern const char kEnableBenchmarking[];
extern const char kEnableClientHints[];
extern const char kEnableBookmarkUndo[];
extern const char kEnableCloudPrintProxy[];
extern const char kEnableDevToolsExperiments[];
extern const char kEnableDeviceDiscoveryNotifications[];
extern const char kEnableDomDistiller[];
extern const char kEnhancedBookmarksExperiment[];
extern const char kEnableDomainReliability[];
extern const char kEnableEnhancedBookmarks[];
extern const char kEnableEphemeralApps[];
extern const char kEnableExtensionActivityLogging[];
extern const char kEnableExtensionActivityLogTesting[];
extern const char kEnableFastUnload[];
extern const char kEnableIPv6[];
extern const char kEnableLinkableEphemeralApps[];
extern const char kEnableNaCl[];
extern const char kEnableNetBenchmarking[];
extern const char kEnableMaterialDesignNTP[];
extern const char kEnableNpnHttpOnly[];
extern const char kEnableOfflineAutoReload[];
extern const char kEnableOfflineAutoReloadVisibleOnly[];
extern const char kEnableOfflineLoadStaleCache[];
extern const char kDisableOfflineLoadStaleCache[];
extern const char kEnableOriginChipAlways[];
extern const char kEnableOriginChipOnSrp[];
extern const char kEnablePanels[];
extern const char kEnablePermissionsBubbles[];
extern const char kEnablePluginPlaceholderShadowDom[];
extern const char kEnablePrintPreviewRegisterPromos[];
extern const char kEnablePrivetStorage[];
extern const char kEnableProfiling[];
extern const char kEnableProminentURLAppFlow[];
extern const char kEnableQueryExtraction[];
extern const char kEnableQuic[];
extern const char kEnableQuicPacing[];
extern const char kEnableQuicPortSelection[];
extern const char kEnableQuicTimeBasedLossDetection[];
extern const char kEnableResourceContentSettings[];
extern const char kEnableSavePasswordBubble[];
extern const char kEnableSdchOverHttps[];
extern const char kEnableSearchButtonInOmniboxAlways[];
extern const char kEnableSearchButtonInOmniboxForStr[];
extern const char kEnableSearchButtonInOmniboxForStrOrIip[];
extern const char kEnableSessionCrashedBubble[];
extern const char kEnableSettingsWindow[];
extern const char kEnableShowModalDialog[];
extern const char kDisableSettingsWindow[];
extern const char kEnableSpdy4[];
extern const char kEnableSpellingAutoCorrect[];
extern const char kEnableSpellingFeedbackFieldTrial[];
extern const char kEnableStreamlinedHostedApps[];
extern const char kEnableSuggestionsService[];
extern const char kEnableSyncArticles[];
extern const char kEnableSyncSyncedNotifications[];
extern const char kEnableThumbnailRetargeting[];
extern const char kEnableTranslateNewUX[];
extern const char kEnableUserAlternateProtocolPorts[];
extern const char kEnableWatchdog[];
extern const char kEnableWebSocketOverSpdy[];
extern const char kEnableWebsiteSettingsManager[];
extern const char kEnhancedBookmarksExperiment[];
extern const char kExplicitlyAllowedPorts[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforceStrict[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerification[];
extern const char kExtensionsInstallVerification[];
extern const char kExtensionsNotWebstore[];
extern const char kExtensionsUpdateFrequency[];
extern const char kFakeVariationsChannel[];
extern const char kFastStart[];
extern const char kFlagSwitchesBegin[];
extern const char kFlagSwitchesEnd[];
extern const char kForceAppMode[];
extern const char kForceFirstRun[];
extern const char kForceVariationIds[];
extern const char kGSSAPILibraryName[];
extern const char kHelp[];
extern const char kHelpShort[];
extern const char kHideIcons[];
extern const char kHistoryEnableGroupByDomain[];
extern const char kHistoryWebHistoryUrl[];
extern const char kHomePage[];
extern const char kHostRules[];
extern const char kHostResolverParallelism[];
extern const char kHostResolverRetryAttempts[];
extern const char kIgnoreUrlFetcherCertRequests[];
extern const char kIncognito[];
extern const char kInstallChromeApp[];
extern const char kInstallFromWebstore[];
extern const char kInstantProcess[];
extern const char kInvalidationUseGCMChannel[];
extern const char kIpcFuzzerTestcase[];
extern const char kKeepAliveForTest[];
extern const char kKioskMode[];
extern const char kKioskModePrinting[];
extern const char kLedgerServer[];
extern const char kLedgerTracingToken[];
extern const char kLimitedInstallFromWebstore[];
extern const char kLoadComponentExtension[];
extern const char kLoadExtension[];
extern const char kMakeDefaultBrowser[];
extern const char kMalwareInterstitialV2[];
extern const char kMalwareInterstitialV3[];
extern const char kMalwareInterstitialV3Advice[];
extern const char kMalwareInterstitialV3Social[];
extern const char kMalwareInterstitialV3NotRecommend[];
extern const char kMalwareInterstitialV3History[];
extern const char kManualEnhancedBookmarks[];
extern const char kManualEnhancedBookmarksOptout[];
extern const char kMediaCacheSize[];
extern const char kMessageLoopHistogrammer[];
extern const char kMetricsRecordingOnly[];
extern const char kNetLogLevel[];
extern const char kNoDefaultBrowserCheck[];
extern const char kNoDisplayingInsecureContent[];
extern const char kNoEvents[];
extern const char kNoExperiments[];
extern const char kNoFirstRun[];
extern const char kNoNetworkProfileWarning[];
extern const char kNoProxyServer[];
extern const char kNoPings[];
extern const char kNoServiceAutorun[];
extern const char kNoStartupWindow[];
extern const char kNoSupervisedUserAcknowledgmentCheck[];
extern const char kNtpAppInstallHint[];
extern const char kNumPacThreads[];
extern const char kOpenInNewWindow[];
extern const char kOriginToForceQuicOn[];
extern const char kOriginalProcessStartTime[];
extern const char kOutOfProcessPdf[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kParentProfile[];
extern const char kPerformanceMonitorGathering[];
extern const char kPermissionRequestApiUrl[];
extern const char kPlaybackMode[];
extern const char kPpapiFlashPath[];
extern const char kPpapiFlashVersion[];
extern const char kPrefetchSearchResults[];
extern const char kPrerenderFromOmnibox[];
extern const char kPrerenderFromOmniboxSwitchValueAuto[];
extern const char kPrerenderFromOmniboxSwitchValueDisabled[];
extern const char kPrerenderFromOmniboxSwitchValueEnabled[];
extern const char kPrerenderMode[];
extern const char kPrerenderModeSwitchValueAuto[];
extern const char kPrerenderModeSwitchValueDisabled[];
extern const char kPrerenderModeSwitchValueEnabled[];
extern const char kPrerenderModeSwitchValuePrefetchOnly[];
extern const char kPrivetIPv6Only[];
extern const char kProductVersion[];
extern const char kProfileDirectory[];
extern const char kProfilingAtStart[];
extern const char kProfilingFile[];
extern const char kProfilingFlush[];
extern const char kProfilingOutputFile[];
extern const char kPromoServerURL[];
extern const char kProxyAutoDetect[];
extern const char kProxyBypassList[];
extern const char kProxyPacUrl[];
extern const char kProxyServer[];
extern const char kQuicConnectionOptions[];
extern const char kQuicMaxPacketLength[];
extern const char kQuicVersion[];
extern const char kRecordMode[];
extern const char kRememberCertErrorDecisions[];
extern const char kRendererPrintPreview[];
extern const char kResetAppListInstallState[];
extern const char kRestoreLastSession[];
extern const char kSavePageAsMHTML[];
extern const char kSbDisableAutoUpdate[];
extern const char kSbDisableDownloadProtection[];
extern const char kSbDisableExtensionBlacklist[];
extern const char kSbDisableSideEffectFreeWhitelist[];
extern const char kServiceProcess[];
extern const char kSilentDebuggerExtensionAPI[];
extern const char kSilentLaunch[];
extern const char kSetToken[];
extern const char kShowAppList[];
extern const char kShowIcons[];
extern const char kSigninProcess[];
extern const char kSimulateUpgrade[];
extern const char kSimulateCriticalUpdate[];
extern const char kSimulateOutdated[];
extern const char kSimulateOutdatedNoAU[];
extern const char kSpellingServiceFeedbackUrl[];
extern const char kSpellingServiceFeedbackIntervalSeconds[];
extern const char kSSLInterstitialV1[];
extern const char kSSLInterstitialV2[];
extern const char kSSLInterstitialV1WithV2Text[];
extern const char kSSLInterstitialV2Yellow[];
extern const char kSSLInterstitialV2Guard[];
extern const char kSSLVersionMax[];
extern const char kSSLVersionMin[];
extern const char kStartMaximized[];
extern const char kSupervisedUserId[];
extern const char kSupervisedUserSyncToken[];
extern const char kSyncShortInitialRetryOverride[];
extern const char kSyncServiceURL[];
extern const char kSyncDisableDeferredStartup[];
extern const char kSyncDeferredStartupTimeoutSeconds[];
extern const char kSyncEnableGetUpdateAvoidance[];
extern const char kSyncDisableBackup[];
extern const char kSyncEnableRollback[];
extern const char kTabCapture[];
extern const char kTestName[];
extern const char kTrustedSpdyProxy[];
extern const char kTryChromeAgain[];
extern const char kUninstall[];
extern const char kUnlimitedStorage[];
extern const char kUseSimpleCacheBackend[];
extern const char kUseSpdy[];
extern const char kUserAgent[];
extern const char kUserDataDir[];
extern const char kValidateCrx[];
extern const char kVariationsServerURL[];
extern const char kVersion[];
extern const char kWindowPosition[];
extern const char kWindowSize[];
extern const char kWinHttpProxyResolver[];

#if defined(ENABLE_PLUGIN_INSTALLATION)
extern const char kPluginsMetadataServerURL[];
#endif

#if defined(OS_ANDROID)
extern const char kDisableCast[];
extern const char kDisableContextualSearch[];
extern const char kDisableZeroSuggest[];
extern const char kEnableAccessibilityTabSwitcher[];
extern const char kEnableAppInstallAlerts[];
extern const char kEnableContextualSearch[];
extern const char kEnableZeroSuggestEtherSerp[];
extern const char kEnableZeroSuggestEtherNoSerp[];
extern const char kEnableZeroSuggestMostVisited[];
extern const char kEnableZeroSuggestPersonalized[];
extern const char kEnableInstantSearchClicks[];
#endif

#if defined(USE_ASH)
extern const char kOpenAsh[];
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
extern const char kPasswordStore[];
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const char kMigrateDataDirForSxS[];
#endif

#if defined(OS_MACOSX)
extern const char kAppsKeepChromeAliveInTests[];
extern const char kDisableSystemFullscreenForTesting[];
extern const char kEnableFullSizeContentView[];
extern const char kEnableSimplifiedFullscreen[];
extern const char kRelauncherProcess[];
#endif

#if defined(OS_WIN)
extern const char kEnableCloudPrintXps[];
extern const char kEnableProfileShortcutManager[];
extern const char kForceDesktop[];
extern const char kForceImmersive[];
extern const char kRelaunchShortcut[];
extern const char kViewerConnect[];
extern const char kViewerLaunchViaAppId[];
extern const char kWaitForMutex[];
extern const char kWindows8Search[];
#endif

#if defined(ENABLE_FULL_PRINTING) && !defined(OFFICIAL_BUILD)
extern const char kDebugPrint[];
#endif

#ifndef NDEBUG
extern const char kFileManagerExtensionPath[];
#endif

bool SettingsWindowEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
