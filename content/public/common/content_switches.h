// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const char kAcceleratedCanvas2dMSAASampleCount[];
CONTENT_EXPORT extern const char kAllowFileAccessFromFiles[];
CONTENT_EXPORT extern const char kAllowInsecureWebSocketFromHttpsOrigin[];
CONTENT_EXPORT extern const char kAllowLoopbackInPeerConnection[];
CONTENT_EXPORT extern const char kAllowNoSandboxJob[];
extern const char kAllowSandboxDebugging[];
extern const char kAuditAllHandles[];
extern const char kAuditHandles[];
CONTENT_EXPORT extern const char kBlinkPlatformLogChannels[];
CONTENT_EXPORT extern const char kBlockCrossSiteDocuments[];
CONTENT_EXPORT extern const char kBrowserAssertTest[];
CONTENT_EXPORT extern const char kBrowserCrashTest[];
CONTENT_EXPORT extern const char kBrowserSubprocessPath[];
extern const char kDebugPluginLoading[];
CONTENT_EXPORT extern const char kDefaultTileWidth[];
CONTENT_EXPORT extern const char kDefaultTileHeight[];
CONTENT_EXPORT extern const char kDisable2dCanvasAntialiasing[];
CONTENT_EXPORT extern const char kDisable3DAPIs[];
CONTENT_EXPORT extern const char kDisableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kDisableLayerSquashing[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideoDecode[];
CONTENT_EXPORT extern const char kDisableApplicationCache[];
extern const char kDisableBackingStoreLimit[];
CONTENT_EXPORT extern const char kDisablePreferCompositingToLCDText[];
CONTENT_EXPORT extern const char kDisableCompositingForTransition[];
CONTENT_EXPORT extern const char kDisableDatabases[];
CONTENT_EXPORT extern const char kDisableDelegatedRenderer[];
extern const char kDisableDesktopNotifications[];
extern const char kDisableDirectNPAPIRequests[];
CONTENT_EXPORT extern const char kDisableDistanceFieldText[];
CONTENT_EXPORT extern const char kDisableDisplayList2dCanvas[];
extern const char kDisableDomainBlockingFor3DAPIs[];
CONTENT_EXPORT extern const char kDisableExperimentalWebGL[];
CONTENT_EXPORT extern const char kDisableFileSystem[];
CONTENT_EXPORT extern const char kDisableFlash3d[];
CONTENT_EXPORT extern const char kDisableFlashStage3d[];
CONTENT_EXPORT extern const char kDisableGpu[];
CONTENT_EXPORT extern const char kDisableGpuCompositing[];
extern const char kDisableGpuProcessCrashLimit[];
CONTENT_EXPORT extern const char kDisableGpuRasterization[];
CONTENT_EXPORT extern const char kDisableGpuSandbox[];
CONTENT_EXPORT extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableLowResTiling[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
extern const char kDisableHistogramCustomizer[];
CONTENT_EXPORT extern const char kDisableImplSidePainting[];
CONTENT_EXPORT extern const char kDisableJava[];
CONTENT_EXPORT extern const char kDisableJavaScript[];
CONTENT_EXPORT extern const char kDisableLCDText[];
CONTENT_EXPORT extern const char kDisablePrefixedEncryptedMedia[];
extern const char kDisableKillAfterBadIPC[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
CONTENT_EXPORT extern const char kDisableMediaSource[];
extern const char kDisablePepper3d[];
CONTENT_EXPORT extern const char kDisablePinch[];
CONTENT_EXPORT extern const char kDisablePlugins[];
CONTENT_EXPORT extern const char kDisablePluginsDiscovery[];
extern const char kDisableRemoteFonts[];
extern const char kDisableRendererAccessibility[];
CONTENT_EXPORT extern const char kDisableSeccompFilterSandbox[];
extern const char kDisableSessionStorage[];
CONTENT_EXPORT extern const char kDisableSetuidSandbox[];
CONTENT_EXPORT extern const char kDisableSharedWorkers[];
CONTENT_EXPORT extern const char kDisableSingleThreadProxyScheduler[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisableSoftwareRasterizer[];
CONTENT_EXPORT extern const char kDisableThreadedCompositing[];
CONTENT_EXPORT extern const char kDisableThreadedScrolling[];
extern const char kDisableV8IdleNotificationAfterCommit[];
CONTENT_EXPORT extern const char kDisableWebSecurity[];
extern const char kDisableXSLT[];
extern const char kDisableXSSAuditor[];
CONTENT_EXPORT extern const char kDisableZeroCopy[];
CONTENT_EXPORT extern const char kDomAutomationController[];
CONTENT_EXPORT extern const char kEnableBleedingEdgeRenderingFastPaths[];
CONTENT_EXPORT extern const char kDisableDeferredFilters[];
CONTENT_EXPORT extern const char kEnableLayerSquashing[];
CONTENT_EXPORT extern const char kEnableContainerCulling[];
CONTENT_EXPORT extern const char kEnableBeginFrameScheduling[];
CONTENT_EXPORT extern const char kEnablePreferCompositingToLCDText[];
CONTENT_EXPORT extern const char kEnableBrowserSideNavigation[];
CONTENT_EXPORT extern const char kEnableCompositingForTransition[];
CONTENT_EXPORT extern const char kEnableDeferredImageDecoding[];
CONTENT_EXPORT extern const char kEnableDelegatedRenderer[];
CONTENT_EXPORT extern const char kEnableDisplayList2dCanvas[];
CONTENT_EXPORT extern const char kEnableDistanceFieldText[];
CONTENT_EXPORT extern const char kEnableDownloadResumption[];
CONTENT_EXPORT extern const char kEnableEncryptedMedia[];
CONTENT_EXPORT extern const char kEnableExperimentalCanvasFeatures[];
CONTENT_EXPORT extern const char kEnableExperimentalWebPlatformFeatures[];
CONTENT_EXPORT extern const char kEnableFileCookies[];
extern const char kEnableGpuClientTracing[];
CONTENT_EXPORT extern const char kEnableGpuRasterization[];
CONTENT_EXPORT extern const char kEnableLowResTiling[];
CONTENT_EXPORT extern const char kEnableImplSidePainting[];
CONTENT_EXPORT extern const char kEnableLCDText[];
CONTENT_EXPORT extern const char kEnableLogging[];
extern const char kEnableMemoryBenchmarking[];
CONTENT_EXPORT extern const char kEnableNetworkInformation[];
CONTENT_EXPORT extern const char kEnableOneCopy[];
CONTENT_EXPORT extern const char kEnableOverlayFullscreenVideo[];
CONTENT_EXPORT extern const char kEnableOverscrollNotifications[];
CONTENT_EXPORT extern const char kEnablePinch[];
CONTENT_EXPORT extern const char kEnablePreciseMemoryInfo[];
CONTENT_EXPORT extern const char kEnableRegionBasedColumns[];
CONTENT_EXPORT extern const char kEnableRendererMojoChannel[];
CONTENT_EXPORT extern const char kEnableSandboxLogging[];
extern const char kEnableSeccompFilterSandbox[];
extern const char kEnableSkiaBenchmarking[];
CONTENT_EXPORT extern const char kEnableSmoothScrolling[];
CONTENT_EXPORT extern const char kEnableSpatialNavigation[];
CONTENT_EXPORT extern const char kEnableStatsTable[];
CONTENT_EXPORT extern const char kEnableStrictSiteIsolation[];
CONTENT_EXPORT extern const char kEnableServiceWorkerSync[];
CONTENT_EXPORT extern const char kEnableTcpFastOpen[];
CONTENT_EXPORT extern const char kEnableThreadedCompositing[];
CONTENT_EXPORT extern const char kEnableTracing[];
CONTENT_EXPORT extern const char kEnableTracingOutput[];
CONTENT_EXPORT extern const char kEnableUserMediaScreenCapturing[];
extern const char kEnableV8ScriptStreaming[];
extern const char kEnableV8IdleNotificationAfterCommit[];
CONTENT_EXPORT extern const char kEnableViewport[];
CONTENT_EXPORT extern const char kEnableViewportMeta[];
CONTENT_EXPORT extern const char kMainFrameResizesAreOrientationChanges[];
CONTENT_EXPORT extern const char kEnableVtune[];
CONTENT_EXPORT extern const char kEnableWebGLDraftExtensions[];
CONTENT_EXPORT extern const char kEnableWebGLImageChromium[];
CONTENT_EXPORT extern const char kEnableWebMIDI[];
CONTENT_EXPORT extern const char kEnableZeroCopy[];
CONTENT_EXPORT extern const char kExtraPluginDir[];
CONTENT_EXPORT extern const char kForceFieldTrials[];
CONTENT_EXPORT extern const char kForceGpuRasterization[];
CONTENT_EXPORT extern const char kForceRendererAccessibility[];
extern const char kGpuDeviceID[];
extern const char kGpuDriverVendor[];
extern const char kGpuDriverVersion[];
extern const char kGpuLauncher[];
CONTENT_EXPORT extern const char kGpuProcess[];
CONTENT_EXPORT extern const char kGpuSandboxAllowSysVShm[];
CONTENT_EXPORT extern const char kGpuSandboxFailuresFatal[];
CONTENT_EXPORT extern const char kGpuSandboxStartEarly[];
CONTENT_EXPORT extern const char kGpuStartupDialog[];
extern const char kGpuVendorID[];
CONTENT_EXPORT extern const char kHostResolverRules[];
CONTENT_EXPORT extern const char kIgnoreCertificateErrors[];
CONTENT_EXPORT extern const char kIgnoreGpuBlacklist[];
extern const char kInProcessGPU[];
CONTENT_EXPORT extern const char kIPCConnectionTimeout[];
CONTENT_EXPORT extern const char kJavaScriptFlags[];
extern const char kLoadPlugin[];
CONTENT_EXPORT extern const char kLogGpuControlListDecisions[];
CONTENT_EXPORT extern const char kLoggingLevel[];
CONTENT_EXPORT extern const char kLogNetLog[];
extern const char kLogPluginMessages[];
extern const char kMaxUntiledLayerHeight[];
extern const char kMaxUntiledLayerWidth[];
extern const char kMemoryMetrics[];
CONTENT_EXPORT extern const char kMuteAudio[];
CONTENT_EXPORT extern const char kNoReferrers[];
CONTENT_EXPORT extern const char kNoSandbox[];
CONTENT_EXPORT extern const char kNumRasterThreads[];
CONTENT_EXPORT extern const char kOverscrollHistoryNavigation[];
extern const char kPluginLauncher[];
CONTENT_EXPORT extern const char kPluginPath[];
CONTENT_EXPORT extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
CONTENT_EXPORT extern const char kPpapiBrokerProcess[];
CONTENT_EXPORT extern const char kPpapiFlashArgs[];
CONTENT_EXPORT extern const char kPpapiInProcess[];
extern const char kPpapiPluginLauncher[];
CONTENT_EXPORT extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
CONTENT_EXPORT extern const char kProcessPerSite[];
CONTENT_EXPORT extern const char kProcessPerTab[];
CONTENT_EXPORT extern const char kProcessType[];
CONTENT_EXPORT extern const char kReduceSecurityForTesting[];
CONTENT_EXPORT extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteDebuggingPort[];
CONTENT_EXPORT extern const char kRendererAssertTest[];
extern const char kRendererCmdPrefix[];
CONTENT_EXPORT extern const char kRendererProcess[];
CONTENT_EXPORT extern const char kRendererProcessLimit[];
CONTENT_EXPORT extern const char kRendererStartupDialog[];
extern const char kSandboxIPCProcess[];
CONTENT_EXPORT extern const char kScrollEndEffect[];
extern const char kShowPaintRects[];
CONTENT_EXPORT extern const char kSingleProcess[];
CONTENT_EXPORT extern const char kSitePerProcess[];
CONTENT_EXPORT extern const char kSkipGpuDataLoading[];
CONTENT_EXPORT extern const char kStartFullscreen[];
CONTENT_EXPORT extern const char kStatsCollectionController[];
CONTENT_EXPORT extern const char kTabCaptureDownscaleQuality[];
CONTENT_EXPORT extern const char kTabCaptureUpscaleQuality[];
CONTENT_EXPORT extern const char kTestingFixedHttpPort[];
CONTENT_EXPORT extern const char kTestingFixedHttpsPort[];
CONTENT_EXPORT extern const char kTestType[];
CONTENT_EXPORT extern const char kTouchScrollingMode[];
CONTENT_EXPORT extern const char kTouchScrollingModeAsyncTouchmove[];
CONTENT_EXPORT extern const char kTouchScrollingModeSyncTouchmove[];
CONTENT_EXPORT extern const char kTouchScrollingModeTouchcancel[];
CONTENT_EXPORT extern const char kTraceShutdown[];
extern const char kTraceShutdownFile[];
extern const char kTraceStartup[];
extern const char kTraceStartupDuration[];
extern const char kTraceStartupFile[];
extern const char kTraceUploadURL[];
CONTENT_EXPORT extern const char kUIPrioritizeInGpuProcess[];
CONTENT_EXPORT extern const char kUseDiscardableMemory[];
CONTENT_EXPORT extern const char kUseFakeUIForMediaStream[];
CONTENT_EXPORT extern const char kUseMobileUserAgent[];
extern const char kUseSurfaces[];
extern const char kUtilityCmdPrefix[];
CONTENT_EXPORT extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
CONTENT_EXPORT extern const char kUtilityProcessEnableMDns[];
CONTENT_EXPORT extern const char kUtilityProcessRunningElevated[];
extern const char kV8CacheOptions[];
CONTENT_EXPORT extern const char kValidateInputEventStream[];
CONTENT_EXPORT extern const char kWaitForDebuggerChildren[];
CONTENT_EXPORT extern const char kZygoteCmdPrefix[];
CONTENT_EXPORT extern const char kZygoteProcess[];

#if defined(ENABLE_WEBRTC)
CONTENT_EXPORT extern const char kDisableAudioTrackProcessing[];
CONTENT_EXPORT extern const char kDisableWebRtcHWDecoding[];
CONTENT_EXPORT extern const char kDisableWebRtcEncryption[];
CONTENT_EXPORT extern const char kDisableWebRtcHWEncoding[];
CONTENT_EXPORT extern const char kEnableWebRtcHWVp8Encoding[];
CONTENT_EXPORT extern const char kEnableWebRtcHWH264Encoding[];
#endif

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const char kDisableGestureRequirementForMediaPlayback[];
CONTENT_EXPORT extern const char kDisableClickDelay[];
CONTENT_EXPORT extern const char kDisableOverscrollEdgeEffect[];
CONTENT_EXPORT extern const char kDisableWebRTC[];
CONTENT_EXPORT extern const char kEnableContentVideoViewPowerSaveBlocker[];
CONTENT_EXPORT extern const char kEnableSpeechRecognition[];
CONTENT_EXPORT extern const char kForceUseOverlayEmbeddedVideo[];
CONTENT_EXPORT extern const char kHideScrollbars[];
extern const char kNetworkCountryIso[];
CONTENT_EXPORT extern const char kRemoteDebuggingSocketName[];
CONTENT_EXPORT extern const char kRendererWaitForJavaDebugger[];
#endif

CONTENT_EXPORT extern const char kDisableWebAudio[];

#if defined(OS_CHROMEOS)
CONTENT_EXPORT extern const char kDisablePanelFitting[];
CONTENT_EXPORT extern const char kDisableVaapiAcceleratedVideoEncode[];
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
CONTENT_EXPORT extern const char kEnableSpeechDispatcher[];
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
extern const char kDisableCoreAnimationPlugins[];
#endif

#if defined(OS_WIN)
// This switch contains the device scale factor passed to certain processes
// like renderers, etc.
CONTENT_EXPORT extern const char kDeviceScaleFactor[];
CONTENT_EXPORT extern const char kDisableDirectWrite[];
CONTENT_EXPORT extern const char kDisableLegacyIntermediateWindow[];
// This switch will be removed when we enable the win32K lockdown process
// mitigation.
CONTENT_EXPORT extern const char kEnableWin32kRendererLockDown[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
