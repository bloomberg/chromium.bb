// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
#pragma once

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace switches {

CONTENT_EXPORT extern const char kAllowFileAccessFromFiles[];
extern const char kAllowSandboxDebugging[];
extern const char kAuditHandles[];
extern const char kAuditAllHandles[];
CONTENT_EXPORT extern const char kBrowserAssertTest[];
CONTENT_EXPORT extern const char kBrowserCrashTest[];
CONTENT_EXPORT extern const char kBrowserSubprocessPath[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kChromeFrame[];
CONTENT_EXPORT extern const char kDisable3DAPIs[];
CONTENT_EXPORT extern const char kDisableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kDisableAcceleratedCompositing[];
CONTENT_EXPORT extern const char kDisableAcceleratedLayers[];
CONTENT_EXPORT extern const char kDisableAcceleratedPlugins[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideo[];
CONTENT_EXPORT extern const char kDisableAltWinstation[];
CONTENT_EXPORT extern const char kDisableApplicationCache[];
extern const char kDisableAudio[];
extern const char kDisableBackingStoreLimit[];
CONTENT_EXPORT extern const char kDisableDatabases[];
extern const char kDisableDataTransferItems[];
extern const char kDisableDesktopNotifications[];
CONTENT_EXPORT extern const char kDisableDeviceOrientation[];
CONTENT_EXPORT extern const char kDisableExperimentalWebGL[];
extern const char kDisableFileSystem[];
extern const char kDisableGeolocation[];
CONTENT_EXPORT extern const char kDisableGLMultisampling[];
extern const char kDisableGLSLTranslator[];
extern const char kDisableGpuDriverBugWorkarounds[];
extern const char kDisableGpuSandbox[];
extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
extern const char kDisableIndexedDatabase[];
CONTENT_EXPORT extern const char kDisableJava[];
CONTENT_EXPORT extern const char kDisableJavaScript[];
extern const char kDisableJavaScriptI18NAPI[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisablePlugins[];
CONTENT_EXPORT extern const char kDisablePopupBlocking[];
extern const char kDisableRendererAccessibility[];
extern const char kDisableSSLFalseStart[];
extern const char kDisableSeccompSandbox[];
extern const char kDisableSessionStorage[];
extern const char kDisableSharedWorkers[];
CONTENT_EXPORT extern const char kDisableSpeechInput[];
extern const char kDisableSpellcheckAPI[];
CONTENT_EXPORT extern const char kDisableWebAudio[];
extern const char kDisableWebSockets[];
extern const char kEnableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kEnableAcceleratedDrawing[];
extern const char kEnableAccessibility[];
extern const char kEnableAccessibilityLogging[];
CONTENT_EXPORT extern const char kEnableCompositingForFixedPosition[];
CONTENT_EXPORT extern const char kEnableDNSCertProvenanceChecking[];
CONTENT_EXPORT extern const char kEnableDeviceMotion[];
CONTENT_EXPORT extern const char kDisableFullScreen[];
extern const char kEnablePointerLock[];
extern const char kEnableGamepad[];
extern const char kEnableGPUPlugin[];
CONTENT_EXPORT extern const char kEnableLogging[];
extern const char kEnableMediaSource[];
extern const char kEnableMediaStream[];
extern const char kEnableMonitorProfile[];
extern const char kEnableOriginBoundCerts[];
extern const char kEnablePreparsedJsCaching[];
CONTENT_EXPORT extern const char kEnablePrivilegedWebGLExtensions[];
extern const char kEnableSSLCachedInfo[];
extern const char kEnableSandboxLogging[];
extern const char kEnableSeccompSandbox[];
CONTENT_EXPORT extern const char kEnableStatsTable[];
extern const char kEnableStrictSiteIsolation[];
CONTENT_EXPORT extern const char kEnableThreadedCompositing[];
CONTENT_EXPORT extern const char kEnableTcpFastOpen[];
CONTENT_EXPORT extern const char kEnableTouchEvents[];
extern const char kEnableVideoFullscreen[];
extern const char kEnableVideoLogging[];
CONTENT_EXPORT extern const char kEnableVideoTrack[];
CONTENT_EXPORT extern const char kEnableWebIntents[];
CONTENT_EXPORT extern const char kExperimentalLocationFeatures[];
extern const char kExtraPluginDir[];
extern const char kForceFieldTestNameAndValue[];
CONTENT_EXPORT extern const char kForceRendererAccessibility[];
extern const char kGpuLauncher[];
CONTENT_EXPORT extern const char kGpuProcess[];
extern const char kGpuStartupDialog[];
CONTENT_EXPORT extern const char kIgnoreGpuBlacklist[];
extern const char kInProcessGPU[];
extern const char kInProcessPlugins[];
CONTENT_EXPORT extern const char kInProcessWebGL[];
CONTENT_EXPORT extern const char kJavaScriptFlags[];
extern const char kLoadPlugin[];
CONTENT_EXPORT extern const char kLoggingLevel[];
extern const char kLogPluginMessages[];
extern const char kHighLatencyAudio[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kNaClBrokerProcess[];
CONTENT_EXPORT extern const char kNaClLoaderProcess[];
// TODO(bradchen): remove kNaClLinuxHelper switch.
// This switch enables the experimental lightweight nacl_helper for Linux.
// It will be going away soon, when the helper is enabled permanently.
extern const char kNaClLinuxHelper[];
extern const char kNoJsRandomness[];
CONTENT_EXPORT extern const char kNoReferrers[];
CONTENT_EXPORT extern const char kNoSandbox[];
CONTENT_EXPORT extern const char kPlaybackMode[];
extern const char kPluginLauncher[];
CONTENT_EXPORT extern const char kPluginPath[];
CONTENT_EXPORT extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
CONTENT_EXPORT extern const char kPpapiBrokerProcess[];
CONTENT_EXPORT extern const char kPpapiFlashArgs[];
CONTENT_EXPORT extern const char kPpapiFlashPath[];
CONTENT_EXPORT extern const char kPpapiFlashVersion[];
CONTENT_EXPORT extern const char kPpapiOutOfProcess[];
extern const char kPpapiPluginLauncher[];
CONTENT_EXPORT extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
extern const char kProcessPerSite[];
CONTENT_EXPORT extern const char kProcessPerTab[];
CONTENT_EXPORT extern const char kProcessType[];
CONTENT_EXPORT extern const char kRecordMode[];
CONTENT_EXPORT extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteShellPort[];
CONTENT_EXPORT extern const char kRendererAssertTest[];
extern const char kRendererCmdPrefix[];
CONTENT_EXPORT extern const char kRendererCrashTest[];
CONTENT_EXPORT extern const char kRendererProcess[];
extern const char kRendererStartupDialog[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kServiceProcess[];
extern const char kShowPaintRects[];
extern const char kSimpleDataSource[];
CONTENT_EXPORT extern const char kSingleProcess[];
CONTENT_EXPORT extern const char kSkipGpuDataLoading[];
CONTENT_EXPORT extern const char kTestSandbox[];
extern const char kTraceStartup[];
extern const char kTraceStartupFile[];
extern const char kTraceStartupDuration[];
extern const char kUnlimitedQuotaForFiles[];
CONTENT_EXPORT extern const char kUserAgent[];
extern const char kUtilityCmdPrefix[];
CONTENT_EXPORT extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
CONTENT_EXPORT extern const char kWaitForDebuggerChildren[];
extern const char kWebCoreLogChannels[];
extern const char kWebWorkerProcessPerCore[];
extern const char kWebWorkerShareProcesses[];
CONTENT_EXPORT extern const char kWorkerProcess[];
CONTENT_EXPORT extern const char kZygoteCmdPrefix[];
CONTENT_EXPORT extern const char kZygoteProcess[];

extern const char kEnableVisualWordMovement[];

#if defined(OS_POSIX) && !defined(OS_MACOSX)
extern const char kScrollPixels[];
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
extern const char kUseSystemSSL[];
#endif

#if !defined(OFFICIAL_BUILD)
CONTENT_EXPORT extern const char kRendererCheckFalseTest[];
#endif

}  // namespace switches

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
