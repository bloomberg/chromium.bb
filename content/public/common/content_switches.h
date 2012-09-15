// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_

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
CONTENT_EXPORT extern const char kDisableAudio[];
extern const char kDisableBackingStoreLimit[];
CONTENT_EXPORT extern const char kDisableDatabases[];
extern const char kDisableDataTransferItems[];
CONTENT_EXPORT extern const char kDisableDeferred2dCanvas[];
extern const char kDisableDesktopNotifications[];
CONTENT_EXPORT extern const char kDisableDeviceOrientation[];
#if defined(OS_ANDROID)
CONTENT_EXPORT extern const char kEnableExperimentalWebGL[];
#else
CONTENT_EXPORT extern const char kDisableExperimentalWebGL[];
#endif
CONTENT_EXPORT extern const char kBlacklistAcceleratedCompositing[];
CONTENT_EXPORT extern const char kBlacklistWebGL[];
extern const char kDisableFileSystem[];
CONTENT_EXPORT extern const char kDisableFlash3d[];
extern const char kDisableFlashFullscreen3d[];
CONTENT_EXPORT extern const char kDisableFlashStage3d[];
CONTENT_EXPORT extern const char kDisableForceCompositingMode[];
extern const char kDisableGeolocation[];
extern const char kDisableGpu[];
CONTENT_EXPORT extern const char kDisableGLMultisampling[];
CONTENT_EXPORT extern const char kDisableGpuProcessPrelaunch[];
extern const char kDisableGpuSandbox[];
extern const char kReduceGpuSandbox[];
extern const char kEnableGpuSandbox[];
extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
extern const char kDisableImageTransportSurface[];
CONTENT_EXPORT extern const char kDisableJava[];
CONTENT_EXPORT extern const char kDisableJavaScript[];
extern const char kDisableJavaScriptI18NAPI[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisablePlugins[];
extern const char kDisablePointerLock[];
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
extern const char kSpeechRecognitionWebserviceKey[];
CONTENT_EXPORT extern const char kDisableThreadedAnimation[];
#if defined(OS_ANDROID)
CONTENT_EXPORT extern const char kEnableWebAudio[];
#else
CONTENT_EXPORT extern const char kDisableWebAudio[];
#endif
extern const char kDisableWebSecurity[];
extern const char kDisableWebSockets[];
extern const char kDisableXSSAuditor[];
CONTENT_EXPORT extern const char kDomAutomationController[];
CONTENT_EXPORT extern const char kEnableAcceleratedPainting[];
CONTENT_EXPORT extern const char kEnableAcceleratedFilters[];
extern const char kEnableAcceleratedPlugins[];
extern const char kEnableAccessibilityLogging[];
CONTENT_EXPORT extern const char kEnableCompositingForFixedPosition[];
extern const char kEnableCssShaders[];
CONTENT_EXPORT extern const char kEnableDeviceMotion[];
extern const char kEnableEncryptedMedia[];
extern const char kEnableExperimentalWebKitFeatures[];
extern const char kEnableFastback[];
CONTENT_EXPORT extern const char kEnableFixedLayout[];
CONTENT_EXPORT extern const char kDisableFullScreen[];
extern const char kEnablePointerLock[];
CONTENT_EXPORT extern const char kEnableTextServicesFramework[];
extern const char kEnableGestureTapHighlight[];
extern const char kEnableGpuBenchmarking[];
CONTENT_EXPORT extern const char kEnableLogging[];
extern const char kDisableMediaSource[];
extern const char kEnableMonitorProfile[];
extern const char kEnablePartialSwap[];
extern const char kEnableUIReleaseFrontSurface[];
extern const char kEnablePinch[];
extern const char kEnablePreparsedJsCaching[];
CONTENT_EXPORT extern const char kEnablePrivilegedWebGLExtensions[];
extern const char kEnablePruneGpuCommandBuffers[];
extern const char kEnableRendererSideMixing[];
extern const char kEnableSSLCachedInfo[];
extern const char kEnableSandboxLogging[];
extern const char kEnableSeccompSandbox[];
CONTENT_EXPORT extern const char kEnableSmoothScrolling[];
CONTENT_EXPORT extern const char kEnableStatsTable[];
extern const char kEnableStrictSiteIsolation[];
CONTENT_EXPORT extern const char kEnableThreadedCompositing[];
CONTENT_EXPORT extern const char kDisableThreadedCompositing[];
CONTENT_EXPORT extern const char kEnableTcpFastOpen[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideoDecode[];
extern const char kEnableViewport[];
CONTENT_EXPORT extern const char kExperimentalLocationFeatures[];
CONTENT_EXPORT extern const char kExtraPluginDir[];
CONTENT_EXPORT extern const char kForceCompositingMode[];
extern const char kForceFieldTrials[];
CONTENT_EXPORT extern const char kForceRendererAccessibility[];
extern const char kGpuDeviceID[];
extern const char kGpuDriverVendor[];
extern const char kGpuDriverVersion[];
extern const char kGpuLauncher[];
CONTENT_EXPORT extern const char kGpuProcess[];
extern const char kGpuStartupDialog[];
extern const char kGpuVendorID[];
CONTENT_EXPORT extern const char kGuestRenderer[];
extern const char kInProcessGPU[];
extern const char kInProcessPlugins[];
CONTENT_EXPORT extern const char kInProcessWebGL[];
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
CONTENT_EXPORT extern const char kPpapiOutOfProcess[];
extern const char kPpapiPluginLauncher[];
CONTENT_EXPORT extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
CONTENT_EXPORT extern const char kProcessPerSite[];
CONTENT_EXPORT extern const char kProcessPerTab[];
CONTENT_EXPORT extern const char kProcessType[];
CONTENT_EXPORT extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteDebuggingPort[];
CONTENT_EXPORT extern const char kRendererAssertTest[];
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
CONTENT_EXPORT extern const char kSimulateTouchScreenWithMouse[];
CONTENT_EXPORT extern const char kSingleProcess[];
CONTENT_EXPORT extern const char kSkipGpuDataLoading[];
extern const char kTapDownDeferralTimeMs[];
CONTENT_EXPORT extern const char kTestSandbox[];
extern const char kTraceStartup[];
extern const char kTraceStartupFile[];
extern const char kTraceStartupDuration[];
CONTENT_EXPORT extern const char kUIPrioritizeInGpuProcess[];
CONTENT_EXPORT extern const char kUserAgent[];
extern const char kUtilityCmdPrefix[];
CONTENT_EXPORT extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
CONTENT_EXPORT extern const char kWaitForDebuggerChildren[];
extern const char kWebCoreLogChannels[];
CONTENT_EXPORT extern const char kWorkerProcess[];
CONTENT_EXPORT extern const char kZygoteCmdPrefix[];
CONTENT_EXPORT extern const char kZygoteProcess[];
CONTENT_EXPORT extern const char kDisableSoftwareRasterizer[];
extern const char kDefaultTileWidth[];
extern const char kDefaultTileHeight[];
extern const char kMaxUntiledLayerWidth[];
extern const char kMaxUntiledLayerHeight[];
CONTENT_EXPORT extern const char kEnableFixedPositionCreatesStackingContext[];
CONTENT_EXPORT extern const char kDisableFixedPositionCreatesStackingContext[];

extern const char kEnableVisualWordMovement[];

#if defined(OS_ANDROID)
extern const char kNetworkCountryIso[];
extern const char kUseMobileUserAgent[];
extern const char kGraphicsMode[];
// Not actual flags, just values: for example, --graphics-mode=compositor
extern const char kGraphicsModeValueBasic[];
extern const char kGraphicsModeValueCompositor[];
#endif

#if defined(OS_POSIX)
extern const char kChildCleanExit[];
#endif

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
