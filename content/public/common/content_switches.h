// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
extern const char kAllowWebUICompositing[];
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
CONTENT_EXPORT extern const char kBlacklistAcceleratedCompositing[];
CONTENT_EXPORT extern const char kBlacklistWebGL[];
extern const char kDisableFileSystem[];
extern const char kDisableGeolocation[];
CONTENT_EXPORT extern const char kDisableGLMultisampling[];
extern const char kDisableGpuSandbox[];
extern const char kReduceGpuSandbox[];
extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
extern const char kDisableImageTransportSurface[];
CONTENT_EXPORT extern const char kDisableInteractiveFormValidation[];
CONTENT_EXPORT extern const char kDisableJava[];
CONTENT_EXPORT extern const char kDisableJavaScript[];
extern const char kDisableJavaScriptI18NAPI[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisablePlugins[];
CONTENT_EXPORT extern const char kDisablePopupBlocking[];
extern const char kDisableRemoteFonts[];
extern const char kDisableRendererAccessibility[];
extern const char kDisableSSLFalseStart[];
extern const char kDisableSeccompSandbox[];
extern const char kDisableSeccompFilterSandbox[];
extern const char kDisableSessionStorage[];
extern const char kDisableSharedWorkers[];
extern const char kDisableSiteSpecificQuirks[];
CONTENT_EXPORT extern const char kDisableSpeechInput[];
CONTENT_EXPORT extern const char kEnableScriptedSpeech[];
CONTENT_EXPORT extern const char kDisableThreadedAnimation[];
CONTENT_EXPORT extern const char kDisableWebAudio[];
extern const char kDisableWebSecurity[];
extern const char kDisableWebSockets[];
extern const char kDisableXSSAuditor[];
CONTENT_EXPORT extern const char kDomAutomationController[];
CONTENT_EXPORT extern const char kEnableAcceleratedPainting[];
CONTENT_EXPORT extern const char kEnableAcceleratedFilters[];
extern const char kEnableAccessibilityLogging[];
extern const char kEnableBrowserPlugin[];
CONTENT_EXPORT extern const char kEnableCompositingForFixedPosition[];
extern const char kEnableCssRegions[];
extern const char kEnableCssShaders[];
CONTENT_EXPORT extern const char kEnableDeferred2dCanvas[];
CONTENT_EXPORT extern const char kEnableDeviceMotion[];
extern const char kEnableEncryptedMedia[];
extern const char kEnableFastback[];
CONTENT_EXPORT extern const char kEnableFixedLayout[];
CONTENT_EXPORT extern const char kDisableFullScreen[];
extern const char kEnablePointerLock[];
extern const char kEnableGamepad[];
CONTENT_EXPORT extern const char kEnableLogging[];
extern const char kEnableMediaSource[];
extern const char kEnablePeerConnection[];
extern const char kEnableMonitorProfile[];
extern const char kEnableOriginBoundCerts[];
extern const char kEnablePartialSwap[];
extern const char kEnablePreparsedJsCaching[];
CONTENT_EXPORT extern const char kEnablePrivilegedWebGLExtensions[];
extern const char kEnablePruneGpuCommandBuffers[];
extern const char kEnableSSLCachedInfo[];
extern const char kEnableSandboxLogging[];
extern const char kEnableSeccompSandbox[];
extern const char kEnableShadowDOM[];
extern const char kEnableStyleScoped[];
CONTENT_EXPORT extern const char kEnableSmoothScrolling[];
CONTENT_EXPORT extern const char kEnableStatsTable[];
extern const char kEnableStrictSiteIsolation[];
CONTENT_EXPORT extern const char kEnableThreadedCompositing[];
CONTENT_EXPORT extern const char kDisableThreadedCompositing[];
CONTENT_EXPORT extern const char kEnableTcpFastOpen[];
CONTENT_EXPORT extern const char kEnableVideoTrack[];
extern const char kEnableViewport[];
CONTENT_EXPORT extern const char kExperimentalLocationFeatures[];
extern const char kExtraPluginDir[];
CONTENT_EXPORT extern const char kForceCompositingMode[];
extern const char kForceFieldTrials[];
CONTENT_EXPORT extern const char kForceRendererAccessibility[];
extern const char kGpuLauncher[];
CONTENT_EXPORT extern const char kGpuProcess[];
extern const char kGpuStartupDialog[];
extern const char kInProcessGPU[];
extern const char kInProcessPlugins[];
CONTENT_EXPORT extern const char kInProcessWebGL[];
CONTENT_EXPORT extern const char kInvertWebContent[];
CONTENT_EXPORT extern const char kJavaScriptFlags[];
extern const char kLoadPlugin[];
CONTENT_EXPORT extern const char kLoggingLevel[];
extern const char kLogPluginMessages[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kNaClBrokerProcess[];
CONTENT_EXPORT extern const char kNaClLoaderProcess[];
CONTENT_EXPORT extern const char kNoReferrers[];
CONTENT_EXPORT extern const char kNoSandbox[];
extern const char kPluginLauncher[];
CONTENT_EXPORT extern const char kPluginPath[];
CONTENT_EXPORT extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
CONTENT_EXPORT extern const char kPpapiBrokerProcess[];
CONTENT_EXPORT extern const char kPpapiFlashPath[];
CONTENT_EXPORT extern const char kPpapiFlashVersion[];
CONTENT_EXPORT extern const char kPpapiOutOfProcess[];
extern const char kPpapiPluginLauncher[];
CONTENT_EXPORT extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
extern const char kProcessPerSite[];
CONTENT_EXPORT extern const char kProcessPerTab[];
CONTENT_EXPORT extern const char kProcessType[];
CONTENT_EXPORT extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteDebuggingPort[];
CONTENT_EXPORT extern const char kRendererAssertTest[];
#if defined(OS_POSIX)
extern const char kRendererCleanExit[];
#endif
extern const char kRendererCmdPrefix[];
CONTENT_EXPORT extern const char kRendererProcess[];
extern const char kRendererProcessLimit[];
extern const char kRendererStartupDialog[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kServiceProcess[];
extern const char kShowCompositedLayerBorders[];
extern const char kShowCompositedLayerTree[];
extern const char kShowFPSCounter[];
extern const char kShowPaintRects[];
CONTENT_EXPORT extern const char kSingleProcess[];
CONTENT_EXPORT extern const char kSkipGpuDataLoading[];
CONTENT_EXPORT extern const char kTestSandbox[];
extern const char kTraceStartup[];
extern const char kTraceStartupFile[];
extern const char kTraceStartupDuration[];
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
CONTENT_EXPORT extern const char kDisableSoftwareRasterizer[];

extern const char kEnableVisualWordMovement[];

#if defined(OS_POSIX) && !defined(OS_MACOSX)
extern const char kScrollPixels[];
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
extern const char kUseSystemSSL[];
#endif

extern const char kEnablePerTilePainting[];

#if defined(USE_AURA)
CONTENT_EXPORT extern const char kFlingTapSuppressMaxDown[];
CONTENT_EXPORT extern const char kFlingTapSuppressMaxGap[];
CONTENT_EXPORT extern const char kTestCompositor[];
#endif

}  // namespace switches

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
