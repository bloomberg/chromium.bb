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
CONTENT_EXPORT extern const char kAgcStartupMinVolume[];
CONTENT_EXPORT extern const char kAllowFileAccessFromFiles[];
CONTENT_EXPORT extern const char kAllowLoopbackInPeerConnection[];
CONTENT_EXPORT extern const char kAllowNoSandboxJob[];
CONTENT_EXPORT extern const char kAllowSandboxDebugging[];
CONTENT_EXPORT extern const char kBlinkSettings[];
CONTENT_EXPORT extern const char kBlinkPlatformLogChannels[];
CONTENT_EXPORT extern const char kBrowserCrashTest[];
CONTENT_EXPORT extern const char kBrowserSubprocessPath[];
CONTENT_EXPORT extern const char kUseCrossProcessFramesForGuests[];
extern const char kDebugPluginLoading[];
CONTENT_EXPORT extern const char kDefaultTileWidth[];
CONTENT_EXPORT extern const char kDefaultTileHeight[];
CONTENT_EXPORT extern const char kDisable2dCanvasAntialiasing[];
CONTENT_EXPORT extern const char kDisable3DAPIs[];
CONTENT_EXPORT extern const char kDisableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kDisableAcceleratedJpegDecoding[];
CONTENT_EXPORT extern const char kDisableAcceleratedMjpegDecode[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideoDecode[];
extern const char kDisableBackingStoreLimit[];
CONTENT_EXPORT extern const char kDisableBlinkFeatures[];
CONTENT_EXPORT extern const char kDisableCompositorAnimationTimelines[];
CONTENT_EXPORT extern const char kDisableDatabases[];
CONTENT_EXPORT extern const char kDisableDelayAgnosticAec[];
extern const char kDisableDirectNPAPIRequests[];
CONTENT_EXPORT extern const char kDisableDistanceFieldText[];
CONTENT_EXPORT extern const char kDisableDisplayList2dCanvas[];
extern const char kDisableDomainBlockingFor3DAPIs[];
CONTENT_EXPORT extern const char kDisableEncryptedMedia[];
CONTENT_EXPORT extern const char kDisableExperimentalWebGL[];
CONTENT_EXPORT extern const char kDisableFeatures[];
CONTENT_EXPORT extern const char kDisableFileSystem[];
CONTENT_EXPORT extern const char kDisableFlash3d[];
CONTENT_EXPORT extern const char kDisableFlashStage3d[];
CONTENT_EXPORT extern const char kDisableGestureRequirementForMediaPlayback[];
CONTENT_EXPORT extern const char kDisableGpu[];
CONTENT_EXPORT extern const char kDisableGpuCompositing[];
CONTENT_EXPORT extern const char kDisableGpuEarlyInit[];
CONTENT_EXPORT extern const char kDisableGpuMemoryBufferCompositorResources[];
CONTENT_EXPORT extern const char kDisableGpuMemoryBufferVideoFrames[];
extern const char kDisableGpuProcessCrashLimit[];
CONTENT_EXPORT extern const char kDisableGpuRasterization[];
CONTENT_EXPORT extern const char kDisableGpuSandbox[];
CONTENT_EXPORT extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableLowResTiling[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
CONTENT_EXPORT extern const char kDisableHideInactiveStackedTabCloseButtons[];
extern const char kDisableHistogramCustomizer[];
CONTENT_EXPORT extern const char kDisableLCDText[];
CONTENT_EXPORT extern const char kDisablePreferCompositingToLCDText[];
CONTENT_EXPORT extern const char kEnablePrefixedEncryptedMedia[];
extern const char kDisableKillAfterBadIPC[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
CONTENT_EXPORT extern const char kDisableMediaSource[];
CONTENT_EXPORT extern const char kDisableNamespaceSandbox[];
CONTENT_EXPORT extern const char kDisableNativeGpuMemoryBuffers[];
CONTENT_EXPORT extern const char kDisableNotifications[];
extern const char kDisablePepper3d[];
CONTENT_EXPORT extern const char kDisablePermissionsAPI[];
CONTENT_EXPORT extern const char kDisablePinch[];
CONTENT_EXPORT extern const char kDisablePluginsDiscovery[];
CONTENT_EXPORT extern const char kDisablePresentationAPI[];
CONTENT_EXPORT extern const char kDisableRGBA4444Textures[];
CONTENT_EXPORT extern const char kDisableReadingFromCanvas[];
extern const char kDisableRemoteFonts[];
extern const char kDisableRendererAccessibility[];
CONTENT_EXPORT extern const char kDisableRendererBackgrounding[];
CONTENT_EXPORT extern const char kDisableSeccompFilterSandbox[];
CONTENT_EXPORT extern const char kDisableSetuidSandbox[];
CONTENT_EXPORT extern const char kDisableSharedWorkers[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisableSoftwareRasterizer[];
CONTENT_EXPORT extern const char kDisableSpeechAPI[];
CONTENT_EXPORT extern const char kDisableThreadedCompositing[];
CONTENT_EXPORT extern const char kDisableThreadedScrolling[];
extern const char kDisableV8IdleTasks[];
CONTENT_EXPORT extern const char kDisableWebSecurity[];
extern const char kDisableXSSAuditor[];
CONTENT_EXPORT extern const char kDisableZeroCopy[];
CONTENT_EXPORT extern const char kDomAutomationController[];
extern const char kEnable2dCanvasClipAntialiasing[];
CONTENT_EXPORT extern const char kEnableAggressiveDOMStorageFlushing[];
CONTENT_EXPORT extern const char kEnableAudioSupportForDesktopShare[];
CONTENT_EXPORT extern const char kEnableCredentialManagerAPI[];
CONTENT_EXPORT extern const char kEnablePreferCompositingToLCDText[];
CONTENT_EXPORT extern const char kEnableBlinkFeatures[];
CONTENT_EXPORT extern const char kEnableBrowserSideNavigation[];
CONTENT_EXPORT extern const char kEnableDisplayList2dCanvas[];
CONTENT_EXPORT extern const char kEnableDistanceFieldText[];
CONTENT_EXPORT extern const char kEnableExperimentalCanvasFeatures[];
CONTENT_EXPORT extern const char kEnableExperimentalWebPlatformFeatures[];
CONTENT_EXPORT extern const char kEnableFeatures[];
CONTENT_EXPORT extern const char kEnableWebBluetooth[];
extern const char kEnableGpuClientTracing[];
CONTENT_EXPORT extern const char kEnableGpuMemoryBufferCompositorResources[];
CONTENT_EXPORT extern const char kEnableGpuMemoryBufferVideoFrames[];
CONTENT_EXPORT extern const char kEnableGpuRasterization[];
CONTENT_EXPORT extern const char kGpuRasterizationMSAASampleCount[];
CONTENT_EXPORT extern const char kEnableLowResTiling[];
CONTENT_EXPORT extern const char kEnableImageColorProfiles[];
CONTENT_EXPORT extern const char kEnableLCDText[];
CONTENT_EXPORT extern const char kEnableLogging[];
extern const char kEnableMemoryBenchmarking[];
CONTENT_EXPORT extern const char kEnableMojoShellConnection[];
CONTENT_EXPORT extern const char kEnableNetworkInformation[];
CONTENT_EXPORT extern const char kEnableNonValidatingReloadOnRefreshContent[];
CONTENT_EXPORT extern const char kEnablePartialRaster[];
CONTENT_EXPORT extern const char kEnablePinch[];
CONTENT_EXPORT extern const char kEnablePluginPlaceholderTesting[];
CONTENT_EXPORT extern const char kEnablePreciseMemoryInfo[];
CONTENT_EXPORT extern const char kEnableRGBA4444Textures[];
CONTENT_EXPORT extern const char kEnableSandboxLogging[];
CONTENT_EXPORT extern const char kEnableScrollAnchoring[];
extern const char kEnableSkiaBenchmarking[];
CONTENT_EXPORT extern const char kEnableSlimmingPaintV2[];
CONTENT_EXPORT extern const char kEnableSmoothScrolling[];
CONTENT_EXPORT extern const char kEnableSpatialNavigation[];
CONTENT_EXPORT extern const char kEnableStaleWhileRevalidate[];
CONTENT_EXPORT extern const char kEnableStatsTable[];
CONTENT_EXPORT extern const char kEnableStrictMixedContentChecking[];
CONTENT_EXPORT extern const char kEnableStrictPowerfulFeatureRestrictions[];
CONTENT_EXPORT extern const char kEnableTcpFastOpen[];
CONTENT_EXPORT extern const char kEnableThreadedCompositing[];
CONTENT_EXPORT extern const char kEnableTracing[];
CONTENT_EXPORT extern const char kEnableTracingOutput[];
CONTENT_EXPORT extern const char kEnableUserMediaScreenCapturing[];
CONTENT_EXPORT extern const char kEnableUseZoomForDSF[];
CONTENT_EXPORT extern const char kEnableViewport[];
CONTENT_EXPORT extern const char kEnableVtune[];
CONTENT_EXPORT extern const char kEnableWebGLDraftExtensions[];
CONTENT_EXPORT extern const char kEnableWebGLImageChromium[];
CONTENT_EXPORT extern const char kEnableWebVR[];
CONTENT_EXPORT extern const char kEnableWheelGestures[];
CONTENT_EXPORT extern const char kEnableZeroCopy[];
CONTENT_EXPORT extern const char kExplicitlyAllowedPorts[];
CONTENT_EXPORT extern const char kExtraPluginDir[];
CONTENT_EXPORT extern const char kForceDisplayList2dCanvas[];
CONTENT_EXPORT extern const char kForceGpuRasterization[];
CONTENT_EXPORT extern const char kForceOverlayFullscreenVideo[];
CONTENT_EXPORT extern const char kForceRendererAccessibility[];
CONTENT_EXPORT extern const char kGenerateAccessibilityTestExpectations[];
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
CONTENT_EXPORT extern const char kInProcessGPU[];
CONTENT_EXPORT extern const char kIPCConnectionTimeout[];
CONTENT_EXPORT extern const char kJavaScriptFlags[];
extern const char kLoadPlugin[];
CONTENT_EXPORT extern const char kLogGpuControlListDecisions[];
CONTENT_EXPORT extern const char kLoggingLevel[];
CONTENT_EXPORT extern const char kLogNetLog[];
extern const char kLogPluginMessages[];
CONTENT_EXPORT extern const char kMainFrameResizesAreOrientationChanges[];
extern const char kMaxUntiledLayerHeight[];
extern const char kMaxUntiledLayerWidth[];
extern const char kMemoryMetrics[];
CONTENT_EXPORT extern const char kMuteAudio[];
CONTENT_EXPORT extern const char kNoReferrers[];
CONTENT_EXPORT extern const char kNoSandbox[];
CONTENT_EXPORT extern const char kEnableAppContainer[];
CONTENT_EXPORT extern const char kDisableAppContainer[];
CONTENT_EXPORT extern const char kNumRasterThreads[];
CONTENT_EXPORT extern const char kOverridePluginPowerSaverForTesting[];
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
CONTENT_EXPORT extern const char kReducedReferrerGranularity[];
CONTENT_EXPORT extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteDebuggingPort[];
extern const char kRendererCmdPrefix[];
CONTENT_EXPORT extern const char kRendererProcess[];
CONTENT_EXPORT extern const char kRendererProcessLimit[];
CONTENT_EXPORT extern const char kRendererStartupDialog[];
CONTENT_EXPORT extern const char kRootLayerScrolls[];
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
CONTENT_EXPORT extern const char kTouchTextSelectionStrategy[];
CONTENT_EXPORT extern const char kUIPrioritizeInGpuProcess[];
CONTENT_EXPORT extern const char kUseFakeUIForMediaStream[];
CONTENT_EXPORT extern const char kUseMusInRenderer[];
CONTENT_EXPORT extern const char kEnableNativeGpuMemoryBuffers[];
CONTENT_EXPORT extern const char kContentImageTextureTarget[];
CONTENT_EXPORT extern const char kVideoImageTextureTarget[];
CONTENT_EXPORT extern const char kUseMobileUserAgent[];
CONTENT_EXPORT extern const char kUseNormalPriorityForTileTaskWorkerThreads[];
CONTENT_EXPORT extern const char kUseRemoteCompositing[];
extern const char kUtilityCmdPrefix[];
CONTENT_EXPORT extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
CONTENT_EXPORT extern const char kUtilityProcessRunningElevated[];
CONTENT_EXPORT extern const char kV8CacheOptions[];
CONTENT_EXPORT extern const char kV8NativesPassedByFD[];
CONTENT_EXPORT extern const char kV8SnapshotPassedByFD[];
CONTENT_EXPORT extern const char kValidateInputEventStream[];
CONTENT_EXPORT extern const char kWaitForDebuggerChildren[];
CONTENT_EXPORT extern const char kZygoteCmdPrefix[];
CONTENT_EXPORT extern const char kZygoteProcess[];

#if defined(ENABLE_WEBRTC)
CONTENT_EXPORT extern const char kDisableWebRtcHWDecoding[];
CONTENT_EXPORT extern const char kDisableWebRtcEncryption[];
CONTENT_EXPORT extern const char kDisableWebRtcHWEncoding[];
CONTENT_EXPORT extern const char kEnableWebRtcDtls12[];
CONTENT_EXPORT extern const char kEnableWebRtcHWH264Encoding[];
CONTENT_EXPORT extern const char kEnableWebRtcStunOrigin[];
CONTENT_EXPORT extern const char kEnforceWebRtcIPPermissionCheck[];
CONTENT_EXPORT extern const char kForceWebRtcIPHandlingPolicy[];
CONTENT_EXPORT extern const char kWebRtcStunProbeTrialParameter[];
extern const char kWebRtcMaxCaptureFramerate[];
#endif

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const char kDisableAndroidCompositorAnimationTimelines[];
CONTENT_EXPORT extern const char kDisableOverscrollEdgeEffect[];
CONTENT_EXPORT extern const char kDisablePullToRefreshEffect[];
CONTENT_EXPORT extern const char kDisableScreenOrientationLock[];
CONTENT_EXPORT extern const char kEnableAdaptiveSelectionHandleOrientation[];
CONTENT_EXPORT extern const char kEnableLongpressDragSelection[];
CONTENT_EXPORT extern const char kIPCSyncCompositing[];
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
extern const char kDisableThreadedEventHandlingMac[];
#endif

#if defined(OS_WIN)
CONTENT_EXPORT extern const char kPrefetchArgumentRenderer[];
CONTENT_EXPORT extern const char kPrefetchArgumentGpu[];
CONTENT_EXPORT extern const char kPrefetchArgumentPpapi[];
CONTENT_EXPORT extern const char kPrefetchArgumentPpapiBroker[];
CONTENT_EXPORT extern const char kPrefetchArgumentOther[];
// This switch contains the device scale factor passed to certain processes
// like renderers, etc.
CONTENT_EXPORT extern const char kDeviceScaleFactor[];
CONTENT_EXPORT extern const char kDisableLegacyIntermediateWindow[];
CONTENT_EXPORT extern const char kDisableWin32kRendererLockDown[];
CONTENT_EXPORT extern const char kEnableWin32kLockDownMimeTypes[];
CONTENT_EXPORT extern const char kEnableAcceleratedVpxDecode[];
// Switch to pass the font cache shared memory handle to the renderer.
CONTENT_EXPORT extern const char kFontCacheSharedHandle[];
CONTENT_EXPORT extern const char kMemoryPressureThresholdsMb[];
CONTENT_EXPORT extern const char kTraceExportEventsToETW[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
