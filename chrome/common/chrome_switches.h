// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_
#pragma once

#include "build/build_config.h"

#include "base/base_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in
// base/base_switches.cc instead.
// -----------------------------------------------------------------------------

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kActivateOnLaunch[];
extern const char kAllowFileAccessFromFiles[];
extern const char kAllowFileAccess[];
extern const char kAllowOutdatedPlugins[];
extern const char kAllowSSLMITMProxies[];
extern const char kAllowSandboxDebugging[];
extern const char kAllowScriptingGallery[];
extern const char kAlwaysAuthorizePlugins[];
extern const char kAlwaysEnableDevTools[];
extern const char kApp[];
extern const char kAppId[];
extern const char kAppsGalleryReturnTokens[];
extern const char kAppsGalleryURL[];
extern const char kAppsGalleryUpdateURL[];
extern const char kAppsNoThrob[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kAuthSchemes[];
extern const char kAuthServerWhitelist[];
extern const char kAutomationClientChannelID[];
extern const char kBlockReadingThirdPartyCookies[];
extern const char kBrowserAssertTest[];
extern const char kBrowserCrashTest[];
extern const char kBrowserSubprocessPath[];
extern const char kCheckForUpdateIntervalSec[];
extern const char kChromeFrame[];
extern const char kChromeVersion[];
extern const char kCloudPrintFile[];
extern const char kCloudPrintJobTitle[];
extern const char kCloudPrintProxyId[];
extern const char kCloudPrintServiceURL[];
extern const char kConflictingModulesCheck[];
extern const char kCountry[];
extern const char kDebugPrint[];
extern const char kDeviceManagementUrl[];
extern const char kDevicePolicyCacheDir[];
extern const char kDiagnostics[];
extern const char kDisable3DAPIs[];
extern const char kDisableAcceleratedCompositing[];
extern const char kDisableAcceleratedLayers[];
extern const char kDisableAcceleratedVideo[];
extern const char kDisableAltWinstation[];
extern const char kDisableApplicationCache[];
extern const char kDisableAudio[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kDisableBackgroundMode[];
extern const char kDisableBackgroundNetworking[];
extern const char kDisableBackingStoreLimit[];
extern const char kDisableBlockContentAnimation[];
extern const char kDisableConnectBackupJobs[];
extern const char kDisableCustomJumpList[];
extern const char kDisableDatabases[];
extern const char kDisableDesktopNotifications[];
extern const char kDisableDevTools[];
extern const char kDisableDeviceOrientation[];
extern const char kDisableEnforcedThrottling[];
extern const char kDisableExperimentalWebGL[];
extern const char kDisableExtensionsFileAccessCheck[];
extern const char kDisableExtensions[];
extern const char kDisableFileSystem[];
extern const char kDisableFlashSandbox[];
extern const char kDisableGLMultisampling[];
extern const char kDisableGLSLTranslator[];
extern const char kDisableGeolocation[];
extern const char kDisableGpuWatchdog[];
extern const char kDisableHangMonitor[];
extern const char kDisableHistoryQuickProvider[];
extern const char kDisableHistoryURLProvider[];
extern const char kDisableInteractiveFormValidation[];
extern const char kDisableInternalFlash[];
extern const char kDisableIndexedDatabase[];
extern const char kDisableIPv6[];
extern const char kDisableJavaScript[];
extern const char kDisableJava[];
extern const char kDisableLocalStorage[];
extern const char kDisableLogging[];
extern const char kDisableNewTabFirstRun[];
extern const char kDisablePlugins[];
extern const char kDisablePopupBlocking[];
extern const char kDisablePreconnect[];
extern const char kDisablePromptOnRepost[];
extern const char kDisableRemoteFonts[];
extern const char kDisableRendererAccessibility[];
extern const char kDisableRestoreBackgroundContents[];
extern const char kDisableSessionStorage[];
extern const char kDisableSharedWorkers[];
extern const char kDisableSiteSpecificQuirks[];
extern const char kDisableSpeechInput[];
extern const char kDisableSSLFalseStart[];
extern const char kDisableSync[];
extern const char kDisableSyncApps[];
extern const char kDisableSyncAutofill[];
extern const char kDisableSyncAutofillProfile[];
extern const char kDisableSyncBookmarks[];
extern const char kDisableSyncExtensions[];
extern const char kDisableSyncPasswords[];
extern const char kDisableSyncPreferences[];
extern const char kDisableSyncThemes[];
extern const char kDisableTabbedOptions[];
extern const char kDisableTabCloseableStateWatcher[];
extern const char kDisableTranslate[];
extern const char kDisableWebResources[];
extern const char kDisableWebSecurity[];
extern const char kDisableWebSockets[];
extern const char kDisableXSSAuditor[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kDnsLogDetails[];
extern const char kDnsPrefetchDisable[];
extern const char kDnsServer[];
extern const char kDomAutomationController[];
extern const char kDumpHistogramsOnExit[];
extern const char kEnableAccelerated2dCanvas[];
extern const char kEnableAcceleratedPlugins[];
extern const char kEnableAccessibility[];
extern const char kEnableAeroPeekTabs[];
extern const char kEnableAuthNegotiatePort[];
extern const char kEnableBenchmarking[];
extern const char kEnableClientSidePhishingDetection[];
extern const char kEnableClientSidePhishingInterstitial[];
extern const char kEnableClearServerData[];
extern const char kEnableClickToPlay[];
extern const char kEnableCloudPrintProxy[];
extern const char kEnableCloudPrint[];
extern const char kEnableCompositeToTexture[];
extern const char kEnableConfirmToQuit[];
extern const char kEnableConnectBackupJobs[];
extern const char kEnableCrxlessWebApps[];
extern const char kEnableDeviceMotion[];
extern const char kEnableDNSCertProvenanceChecking[];
extern const char kEnableDNSSECCerts[];
extern const char kEnableExperimentalExtensionApis[];
extern const char kEnableExtensionTimelineApi[];
extern const char kEnableFastback[];
extern const char kEnableFileCookies[];
extern const char kEnableFileSystemURLScheme[];
extern const char kEnableGPUPlugin[];
extern const char kEnableInBrowserThumbnailing[];
extern const char kEnableIPv6[];
extern const char kEnableJavaScriptI18NAPI[];
extern const char kEnableLogging[];
extern const char kEnableMemoryInfo[];
extern const char kEnableMonitorProfile[];
extern const char kEnableNaCl[];
extern const char kEnableNaClDebug[];
extern const char kEnableNativeWebWorkers[];
extern const char kEnablePreconnect[];
extern const char kEnablePreparsedJsCaching[];
extern const char kEnablePrintPreview[];
extern const char kEnableRemoting[];
extern const char kEnableResourceContentSettings[];
extern const char kEnableSearchProviderApiV2[];
extern const char kEnableSnapStart[];
extern const char kEnableSpeechInput[];
extern const char kEnableStatsTable[];
extern const char kEnableSync[];
extern const char kEnableSyncAutofill[];
extern const char kEnableSyncPreferences[];
extern const char kEnableSyncSessions[];
extern const char kEnableSyncTypedUrls[];
extern const char kEnableTcpFastOpen[];
extern const char kEnableTopSites[];
extern const char kEnableTouch[];
extern const char kEnableVerticalTabs[];
extern const char kEnableVideoFullscreen[];
extern const char kEnableVideoLogging[];
extern const char kEnableWatchdog[];
extern const char kEnableWebAudio[];
// Experimental features.
extern const char kExperimentalLocationFeatures[];
extern const char kExperimentalSpellcheckerFeatures[];
// End experimental features.
extern const char kExplicitlyAllowedPorts[];
extern const char kExtensionProcess[];
extern const char kExtensionsUpdateFrequency[];
extern const char kFlagSwitchesBegin[];
extern const char kFlagSwitchesEnd[];
extern const char kFeedbackServer[];
extern const char kFileDescriptorLimit[];
extern const char kFirstRun[];
extern const char kForceAppsPromoVisible[];
extern const char kForceFieldTestNameAndValue[];
extern const char kForceRendererAccessibility[];
extern const char kForceStubLibcros[];
extern const char kGpuLauncher[];
extern const char kGpuProcess[];
extern const char kGpuStartupDialog[];
extern const char kGSSAPILibraryName[];
extern const char kHelp[];
extern const char kHelpShort[];
extern const char kHideIcons[];
extern const char kHomePage[];
extern const char kHostRules[];
extern const char kHostResolverParallelism[];
extern const char kHostResolverRules[];
extern const char kIgnoreGpuBlacklist[];
extern const char kImport[];
extern const char kImportFromFile[];
extern const char kInProcessPlugins[];
extern const char kInProcessWebGL[];
extern const char kIncognito[];
extern const char kInstantURL[];
extern const char kInternalPepper[];
extern const char kJavaScriptFlags[];
extern const char kKeepAliveForTest[];
extern const char kLoadExtension[];
extern const char kUninstallExtension[];
extern const char kLoadPlugin[];
extern const char kExtraPluginDir[];
extern const char kLogNetLog[];
extern const char kLogPluginMessages[];
extern const char kLoggingLevel[];
extern const char kMakeDefaultBrowser[];
extern const char kMediaCacheSize[];
extern const char kMemoryProfiling[];
extern const char kMessageLoopHistogrammer[];
extern const char kMetricsRecordingOnly[];
extern const char kMinClearSiteDataFlashVersion[];
extern const char kNaClDebugIP[];
extern const char kNaClDebugPorts[];
extern const char kNaClBrokerProcess[];
extern const char kNaClLoaderProcess[];
extern const char kNaClStartupDialog[];
extern const char kNoDefaultBrowserCheck[];
extern const char kNoEvents[];
extern const char kNoExperiments[];
extern const char kNoFirstRun[];
extern const char kNoGpuSandbox[];
extern const char kNoJsRandomness[];
extern const char kNoProxyServer[];
extern const char kNoReferrers[];
extern const char kNoPings[];
extern const char kNoSandbox[];
extern const char kNoStartupWindow[];
extern const char kNotifyCloudPrintTokenExpired[];
extern const char kNumPacThreads[];
extern const char kOpenInNewWindow[];
extern const char kOrganicInstall[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kParentProfile[];
extern const char kPlaybackMode[];
extern const char kPluginDataDir[];
extern const char kPluginLauncher[];
extern const char kPluginPath[];
extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
extern const char kPpapiOutOfProcess[];
extern const char kPpapiPluginLauncher[];
extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
extern const char kPrerender[];
extern const char kPrerenderSwitchValueAuto[];
extern const char kPrerenderSwitchValueDisabled[];
extern const char kPrerenderSwitchValueEnabled[];
extern const char kPrerenderSwitchValuePrefetchOnly[];
extern const char kPrint[];
extern const char kProcessPerSite[];
extern const char kProcessPerTab[];
extern const char kProcessType[];
extern const char kProductVersion[];
extern const char kProfileImportProcess[];
extern const char kProfilingAtStart[];
extern const char kProfilingFile[];
extern const char kProfilingFlush[];
extern const char kProxyAutoDetect[];
extern const char kProxyBypassList[];
extern const char kProxyPacUrl[];
extern const char kProxyServer[];
extern const char kPurgeMemoryButton[];
extern const char kRecordMode[];
extern const char kRegisterPepperPlugins[];
extern const char kReloadKilledTabs[];
extern const char kRemoteDebuggingPort[];
extern const char kRemoteShellPort[];
extern const char kRendererAssertTest[];
extern const char kRendererCmdPrefix[];
extern const char kRendererCrashTest[];
extern const char kRendererProcess[];
extern const char kRendererStartupDialog[];
extern const char kRestoreLastSession[];
extern const char kSafePlugins[];
extern const char kSbInfoURLPrefix[];
extern const char kSbMacKeyURLPrefix[];
extern const char kSbDisableAutoUpdate[];
extern const char kSbEnableDownloadProtection[];
extern const char kSdchFilter[];
extern const char kSearchInOmniboxHint[];
extern const char kServiceProcess[];
extern const char kServiceAccountLsid[];
extern const char kShowCompositedLayerBorders[];
extern const char kShowIcons[];
extern const char kShowPaintRects[];
extern const char kSilentDumpOnDCHECK[];
extern const char kSimpleDataSource[];
extern const char kSingleProcess[];
extern const char kStartMaximized[];
extern const char kSyncAllowInsecureXmppConnection[];
extern const char kSyncInvalidateXmppLogin[];
extern const char kSyncerThreadTimedStop[];
extern const char kSyncNotificationMethod[];
extern const char kSyncNotificationHost[];
extern const char kSyncServiceURL[];
extern const char kSyncTrySsltcpFirstForXmpp[];
extern const char kTestNaClSandbox[];
extern const char kTestName[];
extern const char kTestSandbox[];
extern const char kTestType[];
extern const char kTestingChannelID[];
extern const char kTrustedPlugins[];
extern const char kTryChromeAgain[];
extern const char kUninstall[];
extern const char kUseSpdy[];
extern const char kIgnoreCertificateErrors[];
extern const char kMaxSpdySessionsPerDomain[];
extern const char kMaxSpdyConcurrentStreams[];
extern const char kUnlimitedQuotaForFiles[];
extern const char kUnlimitedQuotaForIndexedDB[];
extern const char kUseLowFragHeapCrt[];
extern const char kUserAgent[];
extern const char kUserDataDir[];
extern const char kUserScriptsDir[];
extern const char kUtilityCmdPrefix[];
extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
extern const char kVersion[];
extern const char kWaitForDebuggerChildren[];
extern const char kWebCoreLogChannels[];
extern const char kWebWorkerProcessPerCore[];
extern const char kWebWorkerShareProcesses[];
extern const char kWinHttpProxyResolver[];
extern const char kWorkerProcess[];
extern const char kZygoteCmdPrefix[];
extern const char kZygoteProcess[];

#if defined(OS_CHROMEOS)
extern const char kEnableGView[];
extern const char kEnableLoginImages[];
extern const char kLoginManager[];
// TODO(avayvod): Remove this flag when it's unnecessary for testing
// purposes.
extern const char kLoginScreen[];
extern const char kLoginScreenSize[];
extern const char kTestLoadLibcros[];
extern const char kLoginProfile[];
extern const char kLoginUser[];
extern const char kLoginPassword[];
extern const char kLoginUserWithNewPassword[];
extern const char kParallelAuth[];
extern const char kChromeosFrame[];
extern const char kCandidateWindowLang[];
extern const char kGuestSession[];
extern const char kStubCros[];
extern const char kScreenSaverUrl[];
extern const char kCompressSystemFeedback[];
extern const char kEnableWebUIMenu[];
extern const char kEnableMediaPlayer[];
extern const char kEnableAdvancedFileSystem[];
#endif

#if defined(OS_LINUX)
extern const char kScrollPixels[];
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
extern const char kUseSystemSSL[];
#endif

#if defined(OS_POSIX)
extern const char kEnableCrashReporter[];
extern const char kNoProcessSingletonDialog[];
#if !defined(OS_MACOSX)
extern const char kPasswordStore[];
#endif
#endif

#if defined(OS_MACOSX)
extern const char kDisableHolePunching[];
extern const char kEnableSandboxLogging[];
#else
extern const char kKioskMode[];
#endif

#if defined(TOOLKIT_VIEWS)
extern const char kDebugViewsPaint[];
#endif

#ifndef NDEBUG
extern const char kClearTokenService[];
extern const char kGearsPluginPathOverride[];
extern const char kSetToken[];
extern const char kWebSocketLiveExperimentHost[];
#endif

#if !defined(OFFICIAL_BUILD)
extern const char kRendererCheckFalseTest[];
#endif

#if defined(HAVE_XINPUT2)
extern const char kTouchDevices[];
#endif

extern const char kDisableSeccompSandbox[];
extern const char kEnableSeccompSandbox[];

// Return true if the switches indicate the seccomp sandbox is enabled.
bool SeccompSandboxEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
