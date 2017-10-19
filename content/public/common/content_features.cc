// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/content_features.h"

namespace features {

// All features in alphabetical order.

// Enables the allowActivationDelegation attribute on iframes.
// https://www.chromestatus.com/features/6025124331388928
const base::Feature kAllowActivationDelegationAttr{
    "AllowActivationDelegationAttr", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables content-initiated, main frame navigations to data URLs.
// TODO(meacer): Remove when the deprecation is complete.
//               https://www.chromestatus.com/feature/5669602927312896
const base::Feature kAllowContentInitiatedDataUrlNavigations{
    "AllowContentInitiatedDataUrlNavigations",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables asm.js to WebAssembly V8 backend.
// http://asmjs.org/spec/latest/
const base::Feature kAsmJsToWebAssembly{"AsmJsToWebAssembly",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables async wheel events.
const base::Feature kAsyncWheelEvents{"AsyncWheelEvents",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Block subresource requests whose URLs contain embedded credentials (e.g.
// `https://user:pass@example.com/resource`).
const base::Feature kBlockCredentialedSubresources{
    "BlockCredentialedSubresources", base::FEATURE_ENABLED_BY_DEFAULT};

// Puts save-data header in the holdback mode. This disables sending of
// save-data header to origins, and to the renderer processes within Chrome.
const base::Feature kDataSaverHoldback{"DataSaverHoldback",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables brotli "Accept-Encoding" advertising and "Content-Encoding" support.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli
const base::Feature kBrotliEncoding{"brotli-encoding",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables browser side navigation (aka PlzNavigate). http://crbug.com/368813
const base::Feature kBrowserSideNavigation{"browser-side-navigation",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Toggles whether the buggy RSA parser is used.
//
// TODO(davidben): Remove this after Chrome 61 is released to
// stable. https://crbug.com/735616.
const base::Feature kBuggyRSAParser{"BuggyRSAParser",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If Canvas2D Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kCanvas2DImageChromium {
  "Canvas2DImageChromium",
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enabled decoding images asynchronously from raster in the renderer
// compositor.
const base::Feature kCheckerImaging{"CheckerImaging",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the compositing of fixed position content that is opaque and can
// preserve LCD text.
const base::Feature kCompositeOpaqueFixedPosition{
    "CompositeOpaqueFixedPosition", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the compositing of scrolling content that supports painting the
// background with the foreground, such that LCD text will still be enabled.
const base::Feature kCompositeOpaqueScrollers{"CompositeOpaqueScrollers",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCompositorImageAnimation{
    "CompositorImageAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables handling touch events in compositor using impl side touch action
// knowledge.
const base::Feature kCompositorTouchAction{"CompositorTouchAction",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Throttle tasks in Blink background timer queues based on CPU budgets
// for the background tab. Bug: https://crbug.com/639852.
const base::Feature kExpensiveBackgroundTimerThrottling{
    "ExpensiveBackgroundTimerThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Feature Policy framework for granting and removing access to
// other features through HTTP headers.
const base::Feature kFeaturePolicy{"FeaturePolicy",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Fetch API keepalive timeout setting.
const base::Feature kFetchKeepaliveTimeoutSetting{
    "FetchKeepaliveTimeoutSetting", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables a blink::FontCache optimization that reuses a font to serve different
// size of font.
const base::Feature kFontCacheScaling{"FontCacheScaling",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a security restriction on iframes navigating their top frame.
// When enabled, the navigation will only be permitted if the iframe is
// same-origin to the top frame, or if a user gesture is being processed.
const base::Feature kFramebustingNeedsSameOriginOrUserGesture{
    "FramebustingNeedsSameOriginOrUserGesture",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables extended Gamepad API features like motion tracking and haptics.
const base::Feature kGamepadExtensions{"GamepadExtensions",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Causes the implementations of guests (inner WebContents) to use
// out-of-process iframes.
const base::Feature kGuestViewCrossProcessFrames{
    "GuestViewCrossProcessFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables BlinkGC heap compaction.
const base::Feature kHeapCompaction{"HeapCompaction",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enable lazy initialization of the media controls.
const base::Feature kLazyInitializeMediaControls{
    "LazyInitializeMediaControls", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables lazily parsing css properties for performance.
const base::Feature kLazyParseCSS{"LazyParseCSS",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Use Mojo IPC for resource loading.
const base::Feature kLoadingWithMojo {
  "LoadingWithMojo",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enables the memory coordinator.
// WARNING:
// The memory coordinator is not ready for use and enabling this may cause
// unexpected memory regression at this point. Please do not enable this.
const base::Feature kMemoryCoordinator {
  "MemoryCoordinator", base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables the network service.
const base::Feature kNetworkService{"NetworkService",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for Web Notification content images.
const base::Feature kNotificationContentImage{"NotificationContentImage",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
// An experiment forcing events to be non-blocking when the main thread is
// deemed unresponsive.  See crbug.com/599609.
const base::Feature kMainThreadBusyScrollIntervention{
    "MainThreadBusyScrollIntervention", base::FEATURE_DISABLED_BY_DEFAULT};

// Blob mojofication.
const base::Feature kMojoBlobs{"MojoBlobs", base::FEATURE_DISABLED_BY_DEFAULT};

// Mojo-based Input Event routing.
const base::Feature kMojoInputMessages{"MojoInputMessages",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables/disables hardware video encode acceleration using Mojo (falls back).
const base::Feature kMojoVideoEncodeAccelerator{
    "MojoVideoEncodeAccelerator", base::FEATURE_ENABLED_BY_DEFAULT};

// ES6 Modules.
const base::Feature kModuleScripts{"ModuleScripts",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// ES6 Modules dynamic imports.
const base::Feature kModuleScriptsDynamicImport{
    "ModuleScriptsDynamicImport", base::FEATURE_ENABLED_BY_DEFAULT};

// Resource fetch optimizations for workers. See crbug.com/443374
const base::Feature kOffMainThreadFetch{"OffMainThreadFetch",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Origin Manifest. See crbug.com/751996
const base::Feature kOriginManifest{"OriginManifest",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Origin Trials for controlling access to feature/API experiments.
const base::Feature kOriginTrials{"OriginTrials",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Out of Blink CORS
const base::Feature kOutOfBlinkCORS{"OutOfBlinkCORS",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Whether a download can be handled by parallel jobs.
const base::Feature kParallelDownloading{
    "ParallelDownloading", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether document level event listeners should default 'passive' to true.
const base::Feature kPassiveDocumentEventListeners{
    "PassiveDocumentEventListeners", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether we should force a touchstart and first touchmove per scroll event
// listeners to be non-blocking during fling.
const base::Feature kPassiveEventListenersDueToFling{
    "PassiveEventListenersDueToFling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Purge+Throttle on platforms except Android and MacOS.
// (Android) Purge+Throttle depends on TabManager, but TabManager doesn't
// support Android. Enable after Android is supported.
// (MacOS X) Enable after Purge+Throttle handles memory pressure signals
// send by OS correctly.
const base::Feature kPurgeAndSuspend {
  "PurgeAndSuspend",
#if defined(OS_MACOSX) || defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// RAF aligned mouse input events support.
const base::Feature kRafAlignedMouseInputEvents{
    "RafAlignedMouseInput", base::FEATURE_ENABLED_BY_DEFAULT};

// If Pepper 3D Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kPepper3DImageChromium {
  "Pepper3DImageChromium",
#if defined(OS_MACOSX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Throttle Blink's rendering pipeline based on frame visibility.
const base::Feature kRenderingPipelineThrottling{
    "RenderingPipelineThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Collect renderer peak memory usage during page loads.
const base::Feature kReportRendererPeakMemoryStats{
    "ReportRendererPeakMemoryStats", base::FEATURE_DISABLED_BY_DEFAULT};

// Loading Dispatcher v0 support with ResourceLoadScheduler (crbug.com/729954).
const base::Feature kResourceLoadScheduler{"ResourceLoadScheduler",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Scrolls to compensate for layout movements (bit.ly/scroll-anchoring).
const base::Feature kScrollAnchoring{"ScrollAnchoring",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Make sendBeacon throw for a Blob with a non simple type.
const base::Feature kSendBeaconThrowForBlobWithNonSimpleType{
    "SendBeaconThrowForBlobWithNonSimpleType",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Service worker based payment apps as defined by w3c here:
// https://w3c.github.io/webpayments-payment-apps-api/
const base::Feature kServiceWorkerPaymentApps{
    "ServiceWorkerPaymentApps", base::FEATURE_DISABLED_BY_DEFAULT};

// Streaming installed scripts on starting service workers.
const base::Feature kServiceWorkerScriptStreaming{
    "ServiceWorkerScriptStreaming", base::FEATURE_ENABLED_BY_DEFAULT};

// An experiment to require process isolation for the sign-in origin,
// https://accounts.google.com.  Launch bug: https://crbug.com/739418.
const base::Feature kSignInProcessIsolation{"sign-in-process-isolation",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Alternative to switches::kSitePerProcess, for turning on full site isolation.
// Launch bug: https://crbug.com/739418.
const base::Feature kSitePerProcess{"site-per-process",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Paint invalidation based on slimming paint. See https://goo.gl/eQczQW
const base::Feature kSlimmingPaintInvalidation{
    "SlimmingPaintInvalidation", base::FEATURE_ENABLED_BY_DEFAULT};

// Throttle Blink timers in out-of-view cross origin frames.
const base::Feature kTimerThrottlingForHiddenFrames{
    "TimerThrottlingForHiddenFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Groups all out-of-process iframes to a different process from the process of
// the top document. This is a performance isolation mode.  Launch bug:
// https://crbug.com/595987.
const base::Feature kTopDocumentIsolation{"top-document-isolation",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables touchpad and wheel scroll latching.
const base::Feature kTouchpadAndWheelScrollLatching{
    "TouchpadAndWheelScrollLatching", base::FEATURE_DISABLED_BY_DEFAULT};

// An experiment to turn off compositing for 2D transform & opacity animations.
const base::Feature kTurnOff2DAndOpacityCompositorAnimations{
    "TurnOff2DAndOpacityCompositorAnimations",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Use Feature Policy to gate the use of permission features like midi,
// geolocation, camera, microphone, etc.
const base::Feature kUseFeaturePolicyForPermissions{
    "UseFeaturePolicyForPermissions", base::FEATURE_DISABLED_BY_DEFAULT};

// Use MojoAudioOutputIPC and RenderFrameAudioOutputStreamFactory rather than
// AudioMessageFilter and AudioRendererHost.
const base::Feature kUseMojoAudioOutputStreamFactory{
    "UseMojoAudioOutputStreamFactory", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether vibrate requires user gesture.
const base::Feature kVibrateRequiresUserGesture{
    "VibrateRequiresUserGesture", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables VR UI.
const base::Feature kVrShell {
  "VrShell",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable WebAssembly structured cloning.
// http://webassembly.org/
const base::Feature kWebAssembly{"WebAssembly",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebAssembly streamed compilation.
const base::Feature kWebAssemblyStreaming{"WebAssemblyStreaming",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enable WebAssembly streamed compilation.
const base::Feature kWebAssemblyTrapHandler{"WebAssemblyTrapHandler",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebAuthentication API is enabled:
// https://w3c.github.io/webauthn
const base::Feature kWebAuth{"WebAuthentication",
                             base::FEATURE_DISABLED_BY_DEFAULT};

// If WebGL Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kWebGLImageChromium{"WebGLImageChromium",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// The JavaScript API for payments on the web.
const base::Feature kWebPayments{"WebPayments",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Makes WebRTC use ECDSA certs by default (i.e., when no cert type was
// specified in JS).
const base::Feature kWebRtcEcdsaDefault {"WebRTC-EnableWebRtcEcdsa",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Use GpuMemoryBuffer backed VideoFrames in media streams.
const base::Feature kWebRtcUseGpuMemoryBufferVideoFrames{
    "WebRTC-UseGpuMemoryBufferVideoFrames",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables HW H264 encoding on Android.
const base::Feature kWebRtcHWH264Encoding{
    "WebRtcHWH264Encoding", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables HW VP8 encoding on Android.
const base::Feature kWebRtcHWVP8Encoding {
  "WebRtcHWVP8Encoding",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Fallback from hardware encoder (if available) to software, for WebRTC
// screensharing that uses temporal scalability.
const base::Feature kWebRtcScreenshareSwEncoding{
    "WebRtcScreenshareSwEncoding", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the WebRTC Echo Canceller version 3 (AEC3). Feature for
// http://crbug.com/688388. This value is sent to WebRTC's echo canceller to
// toggle which echo canceller should be used.
const base::Feature kWebRtcUseEchoCanceller3{"WebRtcUseEchoCanceller3",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
const base::Feature kWebUsb{"WebUSB", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables/disables the Image Capture API.
const base::Feature kImageCaptureAPI{"ImageCaptureAPI",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kKeepAliveRendererForKeepaliveRequests{
    "KeepAliveRendererForKeepaliveRequests", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables WebVR experimental rendering optimizations.
const base::Feature kWebVRExperimentalRendering{
    "WebVRExperimentalRendering", base::FEATURE_DISABLED_BY_DEFAULT};

// Enabled "work stealing" in the script runner.
const base::Feature kWorkStealingInScriptRunner{
    "WorkStealingInScriptRunner", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Autofill Accessibility in Android.
// crbug.com/627860
const base::Feature kAndroidAutofillAccessibility{
    "AndroidAutofillAccessibility", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables hiding incorrectly-sized frames while in fullscreen.
const base::Feature kHideIncorrectlySizedFullscreenFrames{
    "HideIncorrectlySizedFullscreenFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the WebNFC API is enabled:
// https://w3c.github.io/web-nfc/
const base::Feature kWebNfc{"WebNFC", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables WebVR VSync-aligned render loop timing.
const base::Feature kWebVrVsyncAlign{"WebVrVsyncAlign",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
// Enables caching of media devices for the purpose of enumerating them.
const base::Feature kDeviceMonitorMac{"DeviceMonitorMac",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// The V2 sandbox on MacOS removes the unsandboed warmup phase and sandboxes the
// entire life of the process.
const base::Feature kMacV2Sandbox{"MacV2Sandbox",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

// Enables to use a snapshot file in creating V8 contexts.
const base::Feature kV8ContextSnapshot{"V8ContextSnapshot",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsMojoBlobsEnabled() {
  return base::FeatureList::IsEnabled(features::kMojoBlobs) ||
         base::FeatureList::IsEnabled(features::kNetworkService);
}

}  // namespace features
