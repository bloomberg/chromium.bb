// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_

#include "build/build_config.h"

#include "base/base_switches.h"
#include "content/public/common/content_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in
// ui/gl/gl_switches.cc or base/base_switches.cc or
// content/public/common/content_switches.cc or media/base/media_switches.cc
// instead.
// -----------------------------------------------------------------------------

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kAllowCrossOriginAuthPrompt[];
extern const char kAllowFileAccess[];
extern const char kAllowHTTPBackgroundPage[];
extern const char kAllowLegacyExtensionManifests[];
extern const char kAllowNaClSocketAPI[];
extern const char kAllowOutdatedPlugins[];
extern const char kAllowRunningInsecureContent[];
extern const char kAllowScriptingGallery[];
extern const char kAlwaysAuthorizePlugins[];
extern const char kAppId[];
extern const char kApp[];
extern const char kAppNotifyChannelServerURL[];
extern const char kAppsCheckoutURL[];
extern const char kAppsGalleryDownloadURL[];
extern const char kAppsGalleryInstallAutoConfirmForTests[];
extern const char kAppsGalleryReturnTokens[];
extern const char kAppsGalleryURL[];
extern const char kAppsGalleryUpdateURL[];
extern const char kAppsInstallFromManifestURL[];
extern const char kAppsNewInstallBubble[];
extern const char kAppsNoThrob[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kAuthSchemes[];
extern const char kAuthServerWhitelist[];
extern const char kAutoLaunchAtStartup[];
extern const char kAutomationClientChannelID[];
extern const char kAutomationReinitializeOnChannelError[];
extern const char kCheckForUpdateIntervalSec[];
extern const char kCheckCloudPrintConnectorPolicy[];
extern const char kChromeFrameShutdownDelay[];
extern const char kChromeVersion[];
extern const char kCipherSuiteBlacklist[];
extern const char kClearTokenService[];
extern const char kCloudPrintDeleteFile[];
extern const char kCloudPrintFile[];
extern const char kCloudPrintJobTitle[];
extern const char kCloudPrintFileType[];
extern const char kCloudPrintPrintTicket[];
extern const char kCloudPrintProxyId[];
extern const char kCloudPrintServiceURL[];
extern const char kComponentUpdaterDebug[];
extern const char kConflictingModulesCheck[];
extern const char kCountry[];
extern const char kCrashOnHangSeconds[];
extern const char kCrashOnHangThreads[];
extern const char kCrashOnLive[];
extern const char kCreateBrowserOnStartupForTests[];
extern const char kDebugDevToolsFrontend[];
extern const char kDebugEnableFrameToggle[];
extern const char kDebugPrint[];
extern const char kDeviceManagementUrl[];
extern const char kDiagnostics[];
extern const char kDisableAsyncDns[];
extern const char kDisableAsynchronousSpellChecking[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kDisableBackgroundMode[];
extern const char kDisableBackgroundNetworking[];
extern const char kDisableBundledPpapiFlash[];
extern const char kDisableClientSidePhishingDetection[];
extern const char kDisableComponentUpdate[];
extern const char kDisableConnectBackupJobs[];
extern const char kDisableCRLSets[];
extern const char kDisableCustomJumpList[];
extern const char kDisableDefaultApps[];
extern const char kDisableDhcpWpad[];
extern const char kDisableExtensionsFileAccessCheck[];
extern const char kDisableExtensionsHttpThrottling[];
extern const char kDisableExtensionsResourceWhitelist[];
extern const char kDisableExtensions[];
extern const char kDisableFlashSandbox[];
extern const char kDisableImprovedDownloadProtection[];
extern const char kDisableInBrowserThumbnailing[];
extern const char kDisableInfiniteCache[];
extern const char kDisableInternalFlash[];
extern const char kDisableIPv6[];
extern const char kDisableIPPooling[];
extern const char kDisableNonFullscreenMouseLock[];
extern const char kDisableNTPOtherSessionsMenu[];
extern const char kDisablePopupBlocking[];
extern const char kDisablePreconnect[];
extern const char kDisablePromptOnRepost[];
extern const char kDisableRestoreBackgroundContents[];
extern const char kDisableRestoreSessionState[];
extern const char kDisableSync[];
extern const char kDisableSyncAppSettings[];
extern const char kDisableSyncApps[];
extern const char kDisableSyncAppNotifications[];
extern const char kDisableSyncAutofill[];
extern const char kDisableSyncAutofillProfile[];
extern const char kDisableSyncBookmarks[];
extern const char kDisableSyncExtensionSettings[];
extern const char kDisableSyncExtensions[];
extern const char kDisableSyncPasswords[];
extern const char kDisableSyncPreferences[];
extern const char kDisableSyncSearchEngines[];
extern const char kDisableSyncThemes[];
extern const char kDisableSyncTypedUrls[];
extern const char kDisableTranslate[];
extern const char kDisableTLSChannelID[];
extern const char kDisableWebResources[];
extern const char kDisableWebsiteSettings[];
extern const char kDisableZeroBrowsersOpenForTests[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kDnsLogDetails[];
extern const char kDnsPrefetchDisable[];
extern const char kDumpHistogramsOnExit[];
extern const char kEnableActionBox[];
extern const char kEnableAsyncDns[];
extern const char kEnableAuthNegotiatePort[];
extern const char kEnableAutofillFeedback[];
extern const char kEnableAutologin[];
extern const char kEnableBenchmarking[];
extern const char kEnableBundledPpapiFlash[];
extern const char kEnableClientOAuthSignin[];
extern const char kEnableCloudPolicyService[];
extern const char kEnableCloudPrintProxy[];
extern const char kEnableConnectBackupJobs[];
extern const char kEnableContacts[];
extern const char kEnableCrxlessWebApps[];
extern const char kEnableDevToolsExperiments[];
extern const char kEnableDiscoveryInNewTabPage[];
extern const char kEnableEasyOffStoreExtensionInstall[];
extern const char kEnableExperimentalExtensionApis[];
extern const char kEnableExtensionActivityLogging[];
extern const char kEnableExtensionActivityUI[];
extern const char kEnableExtensionsInActionBox[];
extern const char kEnableExtensionTimelineApi[];
extern const char kEnableFramelessConstrainedDialogs[];
extern const char kEnableFileCookies[];
extern const char kEnableHttpPipelining[];
extern const char kEnableInstantExtendedAPI[];
extern const char kEnableIPv6[];
extern const char kEnableIPCFuzzing[];
extern const char kEnableIPPooling[];
extern const char kEnableManagedStorage[];
extern const char kEnableMediaGalleryUI[];
extern const char kEnableMemoryInfo[];
extern const char kEnableMetricsReportingForTesting[];
extern const char kEnableNaCl[];
extern const char kEnableNaClDebug[];
extern const char kEnableNaClExceptionHandling[];
extern const char kEnableNaClIPCProxy[];
extern const char kEnableNpn[];
extern const char kDisableSyncTabs[];
extern const char kEnableNpnHttpOnly[];
extern const char kEnablePanels[];
extern const char kEnablePasswordGeneration[];
extern const char kEnablePnacl[];
extern const char kEnableProfiling[];
extern const char kEnableResourceContentSettings[];
extern const char kEnableRestoreSessionState[];
extern const char kEnableScriptBadges[];
extern const char kEnableSdch[];
extern const char kEnableSpdy3[];
extern const char kEnableSpdyCredentialFrames[];
extern const char kEnableSpdyFlowControl[];
extern const char kEnableStackedTabStrip[];
extern const char kEnableSuggestionsTabPage[];
extern const char kEnableTabGroupsContextMenu[];
extern const char kEnableWatchdog[];
extern const char kEnableWebSocketOverSpdy[];
extern const char kEventPageIdleTime[];
extern const char kEventPageUnloadingTime[];
extern const char kExperimentalSpellcheckerFeatures[];
extern const char kExplicitlyAllowedPorts[];
extern const char kExtensionProcess[];
extern const char kExtensionsUpdateFrequency[];
extern const char kExternalAutofillPopup[];
extern const char kFlagSwitchesBegin[];
extern const char kFlagSwitchesEnd[];
extern const char kFeedbackServer[];
extern const char kFileDescriptorLimit[];
extern const char kFirstRun[];
extern const char kGaiaProfileInfo[];
extern const char kGoogleSearchDomainCheckURL[];
extern const char kGSSAPILibraryName[];
extern const char kHelp[];
extern const char kHelpShort[];
extern const char kHideIcons[];
extern const char kHomePage[];
extern const char kHostRules[];
extern const char kHostResolverParallelism[];
extern const char kHostResolverRetryAttempts[];
extern const char kHostResolverRules[];
extern const char kHstsHosts[];
extern const char kIgnoreGpuBlacklist[];
extern const char kImport[];
extern const char kImportFromFile[];
extern const char kIncognito[];
extern const char kInstantURL[];
extern const char kKeepAliveForTest[];
extern const char kKioskMode[];
extern const char kKioskModePrinting[];
extern const char kLoadComponentExtension[];
extern const char kLoadCloudPolicyOnSignin[];
extern const char kLoadExtension[];
extern const char kLoadOpencryptoki[];
extern const char kUninstallExtension[];
extern const char kLogNetLog[];
extern const char kMakeDefaultBrowser[];
extern const char kManaged[];
extern const char kMediaCacheSize[];
extern const char kMemoryProfiling[];
extern const char kMessageLoopHistogrammer[];
extern const char kMetricsRecordingOnly[];
extern const char kMultiProfiles[];
extern const char kNaClGdb[];
extern const char kNaClGdbScript[];
extern const char kNaClLoaderCmdPrefix[];
extern const char kNetLogLevel[];
extern const char kNoDefaultBrowserCheck[];
extern const char kNoDisplayingInsecureContent[];
extern const char kNoEvents[];
extern const char kNoExperiments[];
extern const char kNoFirstRun[];
extern const char kNoJsRandomness[];
extern const char kNoManaged[];
extern const char kNoNetworkProfileWarning[];
extern const char kNoProxyServer[];
extern const char kNoPings[];
extern const char kNoServiceAutorun[];
extern const char kNoStartupWindow[];
extern const char kNotifyCloudPrintTokenExpired[];
extern const char kNtpAppInstallHint[];
extern const char kNumPacThreads[];
extern const char kOmniboxHistoryQuickProviderNewScoring[];
extern const char kOmniboxHistoryQuickProviderNewScoringEnabled[];
extern const char kOmniboxHistoryQuickProviderNewScoringDisabled[];
extern const char kOmniboxHistoryQuickProviderReorderForInlining[];
extern const char kOmniboxHistoryQuickProviderReorderForInliningEnabled[];
extern const char kOmniboxHistoryQuickProviderReorderForInliningDisabled[];
extern const char kOmniboxInlineHistoryQuickProvider[];
extern const char kOmniboxInlineHistoryQuickProviderAllowed[];
extern const char kOmniboxInlineHistoryQuickProviderProhibited[];
extern const char kOnlyBlockSettingThirdPartyCookies[];
extern const char kOpenInNewWindow[];
extern const char kOrganicInstall[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kParentProfile[];
extern const char kPerformanceMonitorGathering[];
extern const char kPlaybackMode[];
extern const char kPnaclDir[];
extern const char kPpapiFlashFieldTrial[];
extern const char kPpapiFlashFieldTrialDisableByDefault[];
extern const char kPpapiFlashFieldTrialEnableByDefault[];
extern const char kPpapiFlashInProcess[];
extern const char kPpapiFlashPath[];
extern const char kPpapiFlashVersion[];
extern const char kPrerenderFromOmnibox[];
extern const char kPrerenderFromOmniboxSwitchValueAuto[];
extern const char kPrerenderFromOmniboxSwitchValueDisabled[];
extern const char kPrerenderFromOmniboxSwitchValueEnabled[];
extern const char kPrerenderMode[];
extern const char kPrerenderModeSwitchValueAuto[];
extern const char kPrerenderModeSwitchValueDisabled[];
extern const char kPrerenderModeSwitchValueEnabled[];
extern const char kPrerenderModeSwitchValuePrefetchOnly[];
extern const char kProductVersion[];
extern const char kProfileDesktopShortcuts[];
extern const char kProfileDirectory[];
extern const char kProfilingAtStart[];
extern const char kProfilingFile[];
extern const char kProfilingFlush[];
extern const char kProfilingOutputFile[];
extern const char kPromoServerURL[];
extern const char kProtector[];
extern const char kProxyAutoDetect[];
extern const char kProxyBypassList[];
extern const char kProxyPacUrl[];
extern const char kProxyServer[];
extern const char kPurgeMemoryButton[];
extern const char kRecordStats[];
extern const char kRecordMode[];
extern const char kReloadKilledTabs[];
extern const char kRemoteDebuggingFrontend[];
extern const char kRendererPrintPreview[];
extern const char kResetVariationState[];
extern const char kRestoreLastSession[];
extern const char kSavePageAsMHTML[];
extern const char kSbURLPrefix[];
extern const char kSbDisableAutoUpdate[];
extern const char kSbDisableDownloadProtection[];
extern const char kSearchInOmniboxHint[];
extern const char kServiceAccountLsid[];
extern const char kSetToken[];
extern const char kShowAppList[];
extern const char kShowAutofillTypePredictions[];
extern const char kShowComponentExtensionOptions[];
extern const char kShowIcons[];
extern const char kShowLauncherAlignmentMenu[];
extern const char kSilentDumpOnDCHECK[];
extern const char kSimulateUpgrade[];
extern const char kSocketReusePolicy[];
extern const char kSpeculativeResourcePrefetching[];
extern const char kSpeculativeResourcePrefetchingDisabled[];
extern const char kSpeculativeResourcePrefetchingLearning[];
extern const char kSSLVersionMax[];
extern const char kSSLVersionMin[];
extern const char kStartMaximized[];
extern const char kSuggestionNtpFilterWidth[];
extern const char kSuggestionNtpGaussianFilter[];
extern const char kSuggestionNtpLinearFilter[];
extern const char kSyncAllowInsecureXmppConnection[];
extern const char kSyncInvalidateXmppLogin[];
extern const char kSyncKeystoreEncryption[];
extern const char kSyncShortInitialRetryOverride[];
extern const char kSyncNotificationMethod[];
extern const char kSyncNotificationHostPort[];
extern const char kSyncServiceURL[];
extern const char kSyncTabFavicons[];
extern const char kSyncThrowUnrecoverableError[];
extern const char kSyncTrySsltcpFirstForXmpp[];
extern const char kTabBrowserDragging[];
extern const char kTestNaClSandbox[];
extern const char kTestName[];
extern const char kTestType[];
extern const char kTestingChannelID[];
extern const char kTestingFixedHttpPort[];
extern const char kTestingFixedHttpsPort[];
extern const char kTrustedSpdyProxy[];
extern const char kTryChromeAgain[];
extern const char kUninstall[];
extern const char kUseSpdy[];
extern const char kIgnoreCertificateErrors[];
extern const char kMaxSpdySessionsPerDomain[];
extern const char kMaxSpdyConcurrentStreams[];
extern const char kUserDataDir[];
extern const char kVariationsServerURL[];
extern const char kVersion[];
extern const char kVisitURLs[];
extern const char kWhitelistedExtensionID[];
extern const char kWindowPosition[];
extern const char kWindowSize[];
extern const char kWinHttpProxyResolver[];

#if defined(OS_ANDROID)
extern const char kTabletUI[];
#endif

#if defined(OS_CHROMEOS)
// Keep switches in alphabetical order.
extern const char kAshWebUIInit[];
extern const char kDisableBootAnimation[];
extern const char kDisableGData[];
extern const char kDisableHtml5Camera[];
extern const char kDisableLoginAnimations[];
extern const char kDisableNewOobe[];
extern const char kDisableNewWallpaperUI[];
extern const char kDisableOobeAnimation[];
extern const char kEnableBackgroundLoader[];
extern const char kEnableTouchpadThreeFingerClick[];
extern const char kSkipOAuthLogin[];
extern const char kEnableDriveV2Api[];
extern const char kUseLevelDBForGData[];
extern const char kEnableGView[];
extern const char kEnableKioskMode[];
extern const char kEnableRequestTabletSite[];
extern const char kEnableStaticIPConfig[];
extern const char kEnableUnsupportedBluetoothDevices[];
extern const char kFirstBoot[];
extern const char kKioskModeScreensaverPath[];
extern const char kLoginManager[];
// TODO(avayvod): Remove this flag when it's unnecessary for testing
// purposes.
extern const char kLoginScreen[];
extern const char kLoginScreenSize[];
extern const char kLoginProfile[];
extern const char kLoginUser[];
extern const char kLoginPassword[];
extern const char kNoDiscardTabs[];
extern const char kGuestSession[];
extern const char kEchoExtensionPath[];
extern const char kShowVolumeStatus[];
extern const char kStubCrosSettings[];
extern const char kCompressSystemFeedback[];
extern const char kAuthExtensionPath[];
extern const char kEnterpriseEnrollmentInitialModulus[];
extern const char kEnterpriseEnrollmentModulusLimit[];
extern const char kChromeOSReleaseBoard[];
#ifndef NDEBUG
extern const char kOobeSkipPostLogin[];
#endif
#endif

#if defined(OS_POSIX)
extern const char kEnableCrashReporter[];
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
extern const char kPasswordStore[];
#endif
#endif

#if defined(OS_MACOSX)
extern const char kEnableExposeForTabs[];
extern const char kKeychainReauthorize[];
extern const char kRelauncherProcess[];
extern const char kUseMockKeychain[];
#endif

#if defined(OS_WIN)
extern const char kDisableDesktopShortcuts[];
extern const char kEnableSyncCredentialCaching[];
extern const char kForceImmersive[];
extern const char kForceDesktop[];
extern const char kModeSwitch[];
extern const char kPrintRaster[];
extern const char kRelaunchShortcut[];
extern const char kWaitForMutex[];
#endif

#if defined(USE_AURA)
extern const char kDisableTCVA[];
extern const char kOpenAsh[];
#endif

#ifndef NDEBUG
extern const char kFileManagerExtensionPath[];
extern const char kDumpProfileDependencyGraph[];
#endif

#if defined(GOOGLE_CHROME_BUILD)
extern const char kDisablePrintPreview[];
#else
extern const char kEnablePrintPreview[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
