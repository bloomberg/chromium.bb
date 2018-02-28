// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const base::Feature kAllowActivationDelegationAttr;
CONTENT_EXPORT extern const base::Feature
    kAllowContentInitiatedDataUrlNavigations;
CONTENT_EXPORT extern const base::Feature kArrayPrototypeValues;
CONTENT_EXPORT extern const base::Feature kAsmJsToWebAssembly;
CONTENT_EXPORT extern const base::Feature kAsyncWheelEvents;
CONTENT_EXPORT extern const base::Feature kBlockCredentialedSubresources;
CONTENT_EXPORT extern const base::Feature kDataSaverHoldback;
CONTENT_EXPORT extern const base::Feature kBrotliEncoding;
CONTENT_EXPORT extern const base::Feature kNavigationMojoResponse;
CONTENT_EXPORT extern const base::Feature kCanvas2DImageChromium;
CONTENT_EXPORT extern const base::Feature kCheckerImaging;
CONTENT_EXPORT extern const base::Feature kCompositeOpaqueFixedPosition;
CONTENT_EXPORT extern const base::Feature kCompositeOpaqueScrollers;
CONTENT_EXPORT extern const base::Feature kCompositorImageAnimation;
CONTENT_EXPORT extern const base::Feature kCompositorTouchAction;
CONTENT_EXPORT extern const base::Feature kCrossSiteDocumentBlockingAlways;
CONTENT_EXPORT extern const base::Feature kCrossSiteDocumentBlockingIfIsolating;
CONTENT_EXPORT extern const base::Feature kExtendedMouseButtons;
CONTENT_EXPORT extern const base::Feature kExpensiveBackgroundTimerThrottling;
CONTENT_EXPORT extern const base::Feature kFontCacheScaling;
CONTENT_EXPORT extern const base::Feature
    kFramebustingNeedsSameOriginOrUserGesture;
CONTENT_EXPORT extern const base::Feature kGamepadExtensions;
CONTENT_EXPORT extern const base::Feature kGuestViewCrossProcessFrames;
CONTENT_EXPORT extern const base::Feature kHeapCompaction;
CONTENT_EXPORT extern const base::Feature kImageCaptureAPI;
CONTENT_EXPORT extern const base::Feature kIsolateOrigins;
CONTENT_EXPORT extern const char kIsolateOriginsFieldTrialParamName[];
CONTENT_EXPORT extern const base::Feature
    kKeepAliveRendererForKeepaliveRequests;
CONTENT_EXPORT extern const base::Feature kKeyboardLockAPI;
CONTENT_EXPORT extern const base::Feature kLazyInitializeMediaControls;
CONTENT_EXPORT extern const base::Feature kLazyParseCSS;
CONTENT_EXPORT extern const base::Feature kLowPriorityIframes;
CONTENT_EXPORT extern const base::Feature kMediaDevicesSystemMonitorCache;
CONTENT_EXPORT extern const base::Feature kMemoryCoordinator;
CONTENT_EXPORT extern const base::Feature kNetworkServiceInProcess;
CONTENT_EXPORT extern const base::Feature kNotificationContentImage;
CONTENT_EXPORT extern const base::Feature kMainThreadBusyScrollIntervention;
CONTENT_EXPORT extern const base::Feature kMojoInputMessages;
CONTENT_EXPORT extern const base::Feature kMojoSessionStorage;
CONTENT_EXPORT extern const base::Feature kMojoVideoCapture;
CONTENT_EXPORT extern const base::Feature kModuleScriptsDynamicImport;
CONTENT_EXPORT extern const base::Feature kModuleScriptsImportMetaUrl;
CONTENT_EXPORT extern const base::Feature kNotificationsWithMojo;
CONTENT_EXPORT extern const base::Feature kOriginManifest;
CONTENT_EXPORT extern const base::Feature kOriginTrials;
CONTENT_EXPORT extern const base::Feature kParallelDownloading;
CONTENT_EXPORT extern const base::Feature kPassiveDocumentEventListeners;
CONTENT_EXPORT extern const base::Feature kPassiveEventListenersDueToFling;
CONTENT_EXPORT extern const base::Feature kPepper3DImageChromium;
CONTENT_EXPORT extern const base::Feature kPurgeAndSuspend;
CONTENT_EXPORT extern const base::Feature kPWAFullCodeCache;
CONTENT_EXPORT extern const base::Feature kRenderingPipelineThrottling;
CONTENT_EXPORT extern const base::Feature kReportRendererPeakMemoryStats;
CONTENT_EXPORT extern const base::Feature kRequireCSSExtensionForFile;
CONTENT_EXPORT extern const base::Feature kResourceLoadScheduler;
CONTENT_EXPORT extern const base::Feature kRootLayerScrolling;
CONTENT_EXPORT extern const base::Feature
    kRunVideoCaptureServiceInBrowserProcess;
CONTENT_EXPORT extern const base::Feature kScrollAnchoring;
CONTENT_EXPORT extern const base::Feature kScrollAnchorSerialization;
CONTENT_EXPORT
extern const base::Feature kSendBeaconThrowForBlobWithNonSimpleType;
CONTENT_EXPORT extern const base::Feature kServiceWorkerPaymentApps;
CONTENT_EXPORT extern const base::Feature kServiceWorkerScriptFullCodeCache;
CONTENT_EXPORT extern const base::Feature kServiceWorkerServicification;
CONTENT_EXPORT extern const base::Feature kSharedArrayBuffer;
CONTENT_EXPORT extern const base::Feature kSignedHTTPExchange;
CONTENT_EXPORT extern const base::Feature kSignInProcessIsolation;
CONTENT_EXPORT extern const base::Feature kSitePerProcess;
CONTENT_EXPORT extern const base::Feature kSlimmingPaintV175;
CONTENT_EXPORT extern const base::Feature kStopInBackground;
CONTENT_EXPORT extern const base::Feature kStopLoadingInBackground;
CONTENT_EXPORT extern const base::Feature kTimerThrottlingForHiddenFrames;
CONTENT_EXPORT extern const base::Feature kTopDocumentIsolation;
CONTENT_EXPORT extern const base::Feature kTouchpadAndWheelScrollLatching;
CONTENT_EXPORT extern const base::Feature
    kTurnOff2DAndOpacityCompositorAnimations;
CONTENT_EXPORT extern const base::Feature kUnifiedTouchAdjustment;
CONTENT_EXPORT extern const base::Feature kUseFeaturePolicyForPermissions;
CONTENT_EXPORT extern const base::Feature kUseMojoAudioInputStreamFactory;
CONTENT_EXPORT extern const base::Feature kUseMojoAudioOutputStreamFactory;
CONTENT_EXPORT extern const base::Feature kUserActivationV2;
CONTENT_EXPORT extern const base::Feature kV8ContextSnapshot;
CONTENT_EXPORT extern const base::Feature kV8BackgroundCompile;
CONTENT_EXPORT extern const base::Feature kV8VmFuture;
CONTENT_EXPORT extern const base::Feature kVrWebInputEditing;
CONTENT_EXPORT extern const base::Feature kCodeCacheAfterExecute;
CONTENT_EXPORT extern const base::Feature kVibrateRequiresUserGesture;
CONTENT_EXPORT extern const base::Feature kWebAssembly;
CONTENT_EXPORT extern const base::Feature kWebAssemblyStreaming;
CONTENT_EXPORT extern const base::Feature kWebAssemblyTrapHandler;
CONTENT_EXPORT extern const base::Feature kWebAuth;
CONTENT_EXPORT extern const base::Feature kWebGLImageChromium;
CONTENT_EXPORT extern const base::Feature kWebPayments;
CONTENT_EXPORT extern const base::Feature kWebRtcAecBoundedErlSetup;
CONTENT_EXPORT extern const base::Feature kWebRtcAecClockDriftSetup;
CONTENT_EXPORT extern const base::Feature kWebRtcEcdsaDefault;
CONTENT_EXPORT extern const base::Feature kWebRtcHWH264Encoding;
CONTENT_EXPORT extern const base::Feature kWebRtcHWVP8Encoding;
CONTENT_EXPORT extern const base::Feature kWebRtcMultiplexCodec;
CONTENT_EXPORT extern const base::Feature kWebRtcScreenshareSwEncoding;
CONTENT_EXPORT extern const base::Feature kWebRtcUseEchoCanceller3;
CONTENT_EXPORT extern const base::Feature kWebRtcUseGpuMemoryBufferVideoFrames;
CONTENT_EXPORT extern const base::Feature kWebUsb;
CONTENT_EXPORT extern const base::Feature kWebVrVsyncAlign;
CONTENT_EXPORT extern const base::Feature kWebXr;
CONTENT_EXPORT extern const base::Feature kWebXrOrientationSensorDevice;
#if defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kWebXrRenderPath;
CONTENT_EXPORT extern const char kWebXrRenderPathParamName[];
CONTENT_EXPORT extern const char kWebXrRenderPathParamValueClientWait[];
CONTENT_EXPORT extern const char kWebXrRenderPathParamValueGpuFence[];
#endif  // defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kWorkStealingInScriptRunner;

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kAndroidAutofillAccessibility;
CONTENT_EXPORT extern const base::Feature kHideIncorrectlySizedFullscreenFrames;
CONTENT_EXPORT extern const base::Feature kImeThread;
CONTENT_EXPORT extern const base::Feature kWebNfc;
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
CONTENT_EXPORT extern const base::Feature kDeviceMonitorMac;
CONTENT_EXPORT extern const base::Feature kMacV2Sandbox;
#endif  // defined(OS_MACOSX)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForOutOfProcess();
CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForBrowserProcess();

}  // namespace features

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
