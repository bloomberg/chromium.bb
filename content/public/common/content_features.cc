// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/content_features.h"

namespace features {

// All features in alphabetical order.

// Enables content-initiated, main frame navigations to data URLs.
// TODO(meacer): Remove when the deprecation is complete.
//               https://www.chromestatus.com/feature/5669602927312896
const base::Feature kAllowContentInitiatedDataUrlNavigations{
    "AllowContentInitiatedDataUrlNavigations",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables asm.js to WebAssembly V8 backend.
// http://asmjs.org/spec/latest/
const base::Feature kAsmJsToWebAssembly{"AsmJsToWebAssembly",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Block subresource requests whose URLs contain embedded credentials (e.g.
// `https://user:pass@example.com/resource`).
const base::Feature kBlockCredentialedSubresources{
    "BlockCredentialedSubresources", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables brotli "Accept-Encoding" advertising and "Content-Encoding" support.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli
const base::Feature kBrotliEncoding{"brotli-encoding",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables browser side navigation (aka PlzNavigate). http://crbug.com/368813
const base::Feature kBrowserSideNavigation{"browser-side-navigation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// If Canvas2D Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kCanvas2DImageChromium{"Canvas2DImageChromium",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enables handling touch events in compositor using impl side touch action
// knowledge.
const base::Feature kCompositorTouchAction{"CompositorTouchAction",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Speculatively pre-evaluate Javascript which will likely use document.write to
// load an external script. The feature extracts the written markup and sends it
// to the preload scanner.
const base::Feature kDocumentWriteEvaluator{"DocumentWriteEvaluator",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Throttle tasks in Blink background timer queues based on CPU budgets
// for the background tab. Bug: https://crbug.com/639852.
const base::Feature kExpensiveBackgroundTimerThrottling{
    "ExpensiveBackgroundTimerThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Feature Policy framework for granting and removing access to
// other features through HTTP headers.
const base::Feature kFeaturePolicy{"FeaturePolicy",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enable filtering of same-origin tiny plugins
const base::Feature kFilterSameOriginTinyPlugin{
    "FilterSameOriginTinyPlugins", base::FEATURE_DISABLED_BY_DEFAULT};

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
    "GuestViewCrossProcessFrames", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables BlinkGC heap compaction.
const base::Feature kHeapCompaction{"HeapCompaction",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Blink's idle time spell checker.
// Design: https://goo.gl/zONC3v
// Note: The feature is implemented in Blink, and is independent to the
// ENABLE_SPELLCHECK build flag defined in components/spellcheck.
const base::Feature kIdleTimeSpellChecking{"IdleTimeSpellChecking",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables lazily parsing css properties for performance.
const base::Feature kLazyParseCSS{"LazyParseCSS",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Use Mojo IPC for resource loading.
const base::Feature kLoadingWithMojo{"LoadingWithMojo",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Experimental feature to trigger hard-reload on Location.reload().
// crbug.com/716339
const base::Feature kLocationHardReload{"LocationHardReload",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// FeatureList definition for trials to enable the download button on
// MediaDocument.
const base::Feature kMediaDocumentDownloadButton{
    "MediaDocumentDownloadButton",
    base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables the old algorithm for processing video constraints in getUserMedia().
const base::Feature kMediaStreamOldVideoConstraints{
    "MediaStreamOldVideoConstraints", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the memory coordinator.
// WARNING:
// The memory coordinator is not ready for use and enabling this may cause
// unexpected memory regression at this point. Please do not enable this.
const base::Feature kMemoryCoordinator {
  "MemoryCoordinator", base::FEATURE_DISABLED_BY_DEFAULT
};

// Kill switch for Web Notification content images.
const base::Feature kNotificationContentImage{"NotificationContentImage",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
// An experiment forcing events to be non-blocking when the main thread is
// deemed unresponsive.  See crbug.com/599609.
const base::Feature kMainThreadBusyScrollIntervention{
    "MainThreadBusyScrollIntervention", base::FEATURE_DISABLED_BY_DEFAULT};

// Blob mojofication.
const base::Feature kMojoBlobs("MojoBlobs", base::FEATURE_DISABLED_BY_DEFAULT);

// Mojo-based Input Event routing.
const base::Feature kMojoInputMessages("MojoInputMessages",
                                       base::FEATURE_DISABLED_BY_DEFAULT);

// Experimental resource fetch optimizations for workers. See crbug.com/443374
const base::Feature kOffMainThreadFetch{"OffMainThreadFetch",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Origin Trials for controlling access to feature/API experiments.
const base::Feature kOriginTrials{"OriginTrials",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

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

// RAF aligned touch input events support.
const base::Feature kRafAlignedTouchInputEvents{
    "RafAlignedTouchInput", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Require camera/mic requests from pepper plugins to be made from secure
// origins.
const base::Feature kRequireSecureOriginsForPepperMediaRequests{
    "RequireSecureOriginsForPepperMediaRequests",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Scrolls to compensate for layout movements (bit.ly/scroll-anchoring).
const base::Feature kScrollAnchoring{"ScrollAnchoring",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Navigation preload feature of service workers.
const base::Feature kServiceWorkerNavigationPreload{
    "ServiceWorkerNavigationPreload", base::FEATURE_ENABLED_BY_DEFAULT};

// http://tc39.github.io/ecmascript_sharedmem/shmem.html
const base::Feature kSharedArrayBuffer{"SharedArrayBuffer",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// An experiment for skipping compositing small scrollers.
const base::Feature kSkipCompositingSmallScrollers{
    "SkipCompositingSmallScrollers", base::FEATURE_DISABLED_BY_DEFAULT};

// Paint invalidation based on slimming paint. See https://goo.gl/eQczQW
const base::Feature kSlimmingPaintInvalidation{
    "SlimmingPaintInvalidation", base::FEATURE_ENABLED_BY_DEFAULT};

// Throttle Blink timers in out-of-view cross origin frames.
const base::Feature kTimerThrottlingForHiddenFrames{
    "TimerThrottlingForHiddenFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables token binding
// (https://www.ietf.org/id/draft-ietf-tokbind-protocol-04.txt).
const base::Feature kTokenBinding{"token-binding",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Groups all out-of-process iframes to a different process from the process of
// the top document. This is a performance isolation mode.  Launch bug:
// https://crbug.com/595987.
const base::Feature kTopDocumentIsolation{"top-document-isolation",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables touchpad and wheel scroll latching.
const base::Feature kTouchpadAndWheelScrollLatching{
    "TouchpadAndWheelScrollLatching", base::FEATURE_DISABLED_BY_DEFAULT};

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
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebAssembly streamed compilation.
const base::Feature kWebAssemblyTrapHandler{"WebAssemblyTrapHandler",
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

// Enables the WebRTC Echo Canceller version 3 (AEC3). Feature for
// http://crbug.com/688388. This value is sent to WebRTC's echo canceller to
// toggle which echo canceller should be used.
const base::Feature kWebRtcUseEchoCanceller3{"WebRtcUseEchoCanceller3",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
const base::Feature kWebUsb{"WebUSB", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables WebVR experimental rendering optimizations.
const base::Feature kWebVRExperimentalRendering{
    "WebVRExperimentalRendering", base::FEATURE_DISABLED_BY_DEFAULT};

// Make sendBeacon throw for a Blob with a non simple type.
const base::Feature kSendBeaconThrowForBlobWithNonSimpleType{
    "SendBeaconThrowForBlobWithNonSimpleType",
    base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Autofill Accessibility in Android.
// crbug.com/627860
const base::Feature kAndroidAutofillAccessibility{
    "AndroidAutofillAccessibility", base::FEATURE_ENABLED_BY_DEFAULT};

// Service worker based payment apps as defined by w3c here:
// https://w3c.github.io/webpayments-payment-apps-api/
const base::Feature kServiceWorkerPaymentApps{
    "ServiceWorkerPaymentApps",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebNFC API is enabled:
// https://w3c.github.io/web-nfc/
const base::Feature kWebNfc{"WebNFC", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN)
// Emergency "off switch" for new Windows sandbox security mitigation,
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE.
const base::Feature kWinSboxDisableExtensionPoints{
    "WinSboxDisableExtensionPoint", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

}  // namespace features
