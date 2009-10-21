// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_

#include "build/build_config.h"
#include "base/base_switches.h"

namespace switches {

extern const char kDisableHangMonitor[];
extern const char kDisableMetrics[];
extern const char kMetricsRecordingOnly[];
extern const char kBrowserAssertTest[];
extern const char kRendererAssertTest[];
extern const char kBrowserCrashTest[];
extern const char kRendererCrashTest[];
extern const char kRendererStartupDialog[];
extern const char kPluginStartupDialog[];
extern const char kPluginLauncher[];

extern const char kTestingChannelID[];
extern const char kHomePage[];
extern const char kRendererProcess[];
extern const char kZygoteProcess[];
extern const char kBrowserSubprocessPath[];
extern const char kPluginProcess[];
extern const char kWorkerProcess[];
extern const char kNaClProcess[];
extern const char kUtilityProcess[];
extern const char kProfileImportProcess[];
extern const char kSingleProcess[];
extern const char kProcessPerTab[];
extern const char kProcessPerSite[];
extern const char kInProcessPlugins[];
extern const char kNoSandbox[];
extern const char kDisableAltWinstation[];
extern const char kSafePlugins[];
extern const char kTrustedPlugins[];
extern const char kTestSandbox[];
extern const char kUserDataDir[];
extern const char kPluginDataDir[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kMediaCacheSize[];
extern const char kEnableUserDataDirProfiles[];
extern const char kParentProfile[];
extern const char kApp[];
extern const char kDomAutomationController[];
extern const char kPluginPath[];
extern const char kUserAgent[];
extern const char kJavaScriptFlags[];
extern const char kCountry[];
extern const char kWaitForDebuggerChildren[];

extern const char kLogFilterPrefix[];
extern const char kEnableLogging[];
extern const char kLoggingLevel[];
extern const char kLogPluginMessages[];

extern const char kDumpHistogramsOnExit[];
extern const char kDisableLogging[];
extern const char kRemoteShellPort[];
extern const char kUninstall[];
extern const char kOmniBoxPopupCount[];

extern const char kAutomationClientChannelID[];

extern const char kRestoreLastSession[];

extern const char kRecordMode[];
extern const char kPlaybackMode[];
extern const char kNoEvents[];
extern const char kNoJsRandomness[];

extern const char kHideIcons[];
extern const char kShowIcons[];
extern const char kMakeDefaultBrowser[];

extern const char kDisableIPv6[];

extern const char kProxyServer[];
extern const char kNoProxyServer[];
extern const char kProxyBypassList[];
extern const char kProxyAutoDetect[];
extern const char kProxyPacUrl[];
extern const char kWinHttpProxyResolver[];
extern const char kDebugPrint[];
extern const char kPrint[];

extern const char kDnsLogDetails[];
extern const char kDnsPrefetchDisable[];

#if defined(OS_LINUX)
extern const char kAutoSSLClientAuth[];
#endif

extern const char kDisableDevTools[];
extern const char kAlwaysEnableDevTools[];
extern const char kEnableExtensionTimelineApi[];

extern const char kTabCountToLoadOnSessionRestore[];

extern const char kMemoryProfiling[];
extern const char kPurgeMemoryButton[];

extern const char kEnableFileCookies[];

extern const char kStartMaximized[];

extern const char kEnableWatchdog[];

extern const char kFirstRun[];

extern const char kNoFirstRun[];

#if defined(OS_POSIX)
extern const char kNoProcessSingletonDialog[];
#endif

extern const char kMessageLoopHistogrammer[];

extern const char kImport[];

extern const char kSilentDumpOnDCHECK[];

extern const char kDisablePromptOnRepost[];

extern const char kDisablePopupBlocking[];
extern const char kDisableJavaScript[];
extern const char kDisableJava[];
extern const char kDisablePlugins[];
extern const char kDisableImages[];
extern const char kDisableWebSecurity[];
extern const char kEnableRemoteFonts[];

extern const char kUseLowFragHeapCrt[];

extern const char kInternalNaCl[];

#ifndef NDEBUG
extern const char kGearsPluginPathOverride[];
#endif

extern const char kEnableFastback[];

extern const char kDisableSync[];
extern const char kSyncerThreadTimedStop[];

extern const char kSdchFilter[];

extern const char kEnableUserScripts[];
extern const char kDisableExtensions[];
extern const char kExtensionsUpdateFrequency[];
extern const char kLoadExtension[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kShowExtensionsOnTop[];
extern const char kLoadPlugin[];
extern const char kUserScriptsDir[];

extern const char kIncognito[];

extern const char kEnableRendererAccessibility[];

extern const char kTestName[];

extern const char kRendererCmdPrefix[];

extern const char kUtilityCmdPrefix[];

extern const char kWininetFtp[];

extern const char kEnableNativeWebWorkers[];
extern const char kWebWorkerProcessPerCore[];
extern const char kWebWorkerShareProcesses[];

extern const char kBookmarkMenu[];

extern const char kEnableStatsTable[];

extern const char kExperimentalSpellcheckerFeatures[];

extern const char kDisableAudio[];
extern const char kSimpleDataSource[];

extern const char kForceFieldTestNameAndValue[];

extern const char kNewTabPage[];
extern const char kDisableNewTabFirstRun[];

extern const char kDisableWebResources[];

extern const char kEnableBenchmarking[];

extern const char kNoDefaultBrowserCheck[];

extern const char kPrivacyBlacklist[];

extern const char kZygoteCmdPrefix[];

extern const char kThumbnailStore[];

extern const char kTryChromeAgain[];

extern const char kFileDescriptorLimit[];

extern const char kEnableMonitorProfile[];

extern const char kDisableXSSAuditor[];

extern const wchar_t kUseFlip[];

#if defined(OS_POSIX)
extern const char kEnableCrashReporter[];
#endif

extern const char kEnableTabtastic2[];

extern const char kPinnedTabCount[];

extern const char kSearchInOmniboxHint[];

extern const char kEnableLocalStorage[];

extern const char kEnableSessionStorage[];

extern const char kAllowSandboxDebugging[];

#if defined(OS_MACOSX)
extern const char kEnableSandboxLogging[];
#endif

extern const char kEnableSeccompSandbox[];

extern const char kDiagnostics[];

extern const char kDisableCustomJumpList[];

extern const char kEnableDatabases[];

extern const char kEnableApplicationCache[];

extern const char kSyncServiceURL[];

#if defined(OS_CHROMEOS)
extern const char kCookiePipe[];
extern const char kEnableGView[];
#endif

extern const char kDisableByteRangeSupport[];

extern const char kExplicitlyAllowedPorts[];

extern const char kActivateOnLaunch[];
extern const char kEnableWebSockets[];

extern const char kEnableExperimentalWebGL[];

extern const char kEnableDesktopNotifications[];

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
