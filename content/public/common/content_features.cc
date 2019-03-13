// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_features.h"
#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/public/cros_features.h"
#endif

namespace features {

// All features in alphabetical order.

// Enables the allowActivationDelegation attribute on iframes.
// https://www.chromestatus.com/features/6025124331388928
//
// TODO(mustaq): Deprecated, see kUserActivationPostMessageTransfer.
const base::Feature kAllowActivationDelegationAttr{
    "AllowActivationDelegationAttr", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows starting services without the browser process.
const base::Feature kAllowStartingServiceManagerOnly{
    "AllowStartingServiceManagerOnly", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables content-initiated, main frame navigations to data URLs.
// TODO(meacer): Remove when the deprecation is complete.
//               https://www.chromestatus.com/feature/5669602927312896
const base::Feature kAllowContentInitiatedDataUrlNavigations{
    "AllowContentInitiatedDataUrlNavigations",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Accepts Origin-Signed HTTP Exchanges to be signed with certificates
// that do not have CanSignHttpExchangesDraft extension.
// TODO(https://crbug.com/862003): Remove when certificates with
// CanSignHttpExchangesDraft extension are available from trusted CAs.
const base::Feature kAllowSignedHTTPExchangeCertsWithoutExtension{
    "AllowSignedHTTPExchangeCertsWithoutExtension",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Creates audio output and input streams using the audio service.
const base::Feature kAudioServiceAudioStreams{
    "AudioServiceAudioStreams", base::FEATURE_DISABLED_BY_DEFAULT};

// Launches the audio service on the browser startup.
const base::Feature kAudioServiceLaunchOnStartup{
    "AudioServiceLaunchOnStartup", base::FEATURE_DISABLED_BY_DEFAULT};

// Runs the audio service in a separate process.
const base::Feature kAudioServiceOutOfProcess{
    "AudioServiceOutOfProcess", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables await taking 1 tick on the microtask queue.
const base::Feature kAwaitOptimization{"AwaitOptimization",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Kill switch for Background Fetch.
const base::Feature kBackgroundFetch{"BackgroundFetch",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable using the BackForwardCache.
const base::Feature kBackForwardCache{"BackForwardCache",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable incremental marking for Blink's heap managed by the Oilpan garbage
// collector.
const base::Feature kBlinkHeapIncrementalMarking{
    "BlinkHeapIncrementalMarking", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable unified garbage collection in Blink.
const base::Feature kBlinkHeapUnifiedGarbageCollection{
    "BlinkHeapUnifiedGarbageCollection", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable bloated renderer detection.
const base::Feature kBloatedRendererDetection{
    "BloatedRendererDetection", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows swipe left/right from touchpad change browser navigation. Currently
// only enabled by default on CrOS.
const base::Feature kTouchpadOverscrollHistoryNavigation {
  "TouchpadOverscrollHistoryNavigation",
#if defined(OS_CHROMEOS) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Block subresource requests whose URLs contain embedded credentials (e.g.
// `https://user:pass@example.com/resource`).
const base::Feature kBlockCredentialedSubresources{
    "BlockCredentialedSubresources", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables brotli "Accept-Encoding" advertising and "Content-Encoding" support.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli
const base::Feature kBrotliEncoding{"brotli-encoding",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables code caching for inline scripts.
const base::Feature kCacheInlineScriptCode{"CacheInlineScriptCode",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// If Canvas2D Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kCanvas2DImageChromium {
  "Canvas2DImageChromium",
#if defined(OS_MACOSX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// When CollectLiveNonNodeWrappers is enabled, live non-Node wrappers that are
// re-creatable will get collected by V8's minor garbage collector.
const base::Feature kCollectLiveNonNodeWrappers{
    "CollectLiveNonNodeWrappers", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the compositing of fixed position content that is opaque and can
// preserve LCD text.
const base::Feature kCompositeOpaqueFixedPosition{
    "CompositeOpaqueFixedPosition", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the compositing of scrolling content that supports painting the
// background with the foreground, such that LCD text will still be enabled.
const base::Feature kCompositeOpaqueScrollers{"CompositeOpaqueScrollers",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables crash reporting via Reporting API.
// https://www.w3.org/TR/reporting/#crash-report
const base::Feature kCrashReporting{"CrashReporting",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables specification of a target element in the fragment identifier
// via a CSS selector.
const base::Feature kCSSFragmentIdentifiers{"CSSFragmentIdentifiers",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Puts save-data header in the holdback mode. This disables sending of
// save-data header to origins, and to the renderer processes within Chrome.
const base::Feature kDataSaverHoldback{"DataSaverHoldback",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable changing source dynamically for desktop capture.
const base::Feature kDesktopCaptureChangeSource{
    "DesktopCaptureChangeSource", base::FEATURE_ENABLED_BY_DEFAULT};

// When a screen reader is detected, allow users the option of letting
// Google provide descriptions for unlabeled images.
const base::Feature kExperimentalAccessibilityLabels{
    "ExperimentalAccessibilityLabels", base::FEATURE_ENABLED_BY_DEFAULT};

// Throttle tasks in Blink background timer queues based on CPU budgets
// for the background tab. Bug: https://crbug.com/639852.
const base::Feature kExpensiveBackgroundTimerThrottling{
    "ExpensiveBackgroundTimerThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables exposing back/forward mouse buttons to the renderer and the web.
const base::Feature kExtendedMouseButtons{"ExtendedMouseButtons",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled Feature Policy propagation is similar to sandbox flags and,
// sandbox flags are implemented on top of Feature Policy.
const base::Feature kFeaturePolicyForSandbox{"FeaturePolicyForSandbox",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a blink::FontCache optimization that reuses a font to serve different
// size of font.
const base::Feature kFontCacheScaling{"FontCacheScaling",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fixes for matching src: local() for web fonts correctly against full
// font name or postscript name. Rolling out behind a flag, as enabling this
// enables a font indexer on Android which we need to test in the field first.
const base::Feature kFontSrcLocalMatching{"FontSrcLocalMatching",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a security restriction on iframes navigating their top frame.
// When enabled, the navigation will only be permitted if the iframe is
// same-origin to the top frame, or if a user gesture is being processed.
const base::Feature kFramebustingNeedsSameOriginOrUserGesture{
    "FramebustingNeedsSameOriginOrUserGesture",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables freezing frame support based on feature policies.
const base::Feature kFreezeFramesOnVisibility{
    "FreezeFramesOnVisibility", base::FEATURE_DISABLED_BY_DEFAULT};

// Puts network quality estimate related Web APIs in the holdback mode. When the
// holdback is enabled the related Web APIs return network quality estimate
// set by the experiment (regardless of the actual quality).
const base::Feature kNetworkQualityEstimatorWebHoldback{
    "NetworkQualityEstimatorWebHoldback", base::FEATURE_DISABLED_BY_DEFAULT};

// When WebXR Device API is enabled, exposes VR controllers as Gamepads and
// enables additional Gamepad attributes for use with WebXR Device API. Each
// XRInputSource will have a corresponding Gamepad instance.
const base::Feature kWebXrGamepadSupport{"WebXRGamepadSupport",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Causes the implementations of guests (inner WebContents) to use
// out-of-process iframes.
// TODO(533069): Remove once BrowserPlugin is removed.
const base::Feature kGuestViewCrossProcessFrames{
    "GuestViewCrossProcessFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables BlinkGC heap compaction.
const base::Feature kHeapCompaction{"HeapCompaction",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If a page does a client side redirect or adds to the history without a user
// gesture, then skip it on back/forward UI.
const base::Feature kHistoryManipulationIntervention{
    "HistoryManipulationIntervention", base::FEATURE_DISABLED_BY_DEFAULT};

// This flag is used to set field parameters to choose predictor we use when
// kResamplingInputEvents is disabled. It's used for gatherig accuracy metrics
// on finch and also for choosing predictor type for predictedEvents API without
// enabling resampling. It does not have any effect when the resampling flag is
// enabled.
const base::Feature kInputPredictorTypeChoice{
    "InputPredictorTypeChoice", base::FEATURE_DISABLED_BY_DEFAULT};

// Alternative to switches::kIsolateOrigins, for turning on origin isolation.
// List of origins to isolate has to be specified via
// kIsolateOriginsFieldTrialParamName.
const base::Feature kIsolateOrigins{"IsolateOrigins",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const char kIsolateOriginsFieldTrialParamName[] = "OriginsList";

const base::Feature kBuiltInModuleKvStorage{"BuiltInModuleKvStorage",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBuiltInModuleAll{"BuiltInModuleAll",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBuiltInModuleInfra{"BuiltInModuleInfra",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLazyFrameLoading{"LazyFrameLoading",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kLazyFrameVisibleLoadTimeMetrics{
    "LazyFrameVisibleLoadTimeMetrics", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kLazyImageLoading{"LazyImageLoading",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kLazyImageVisibleLoadTimeMetrics{
    "LazyImageVisibleLoadTimeMetrics", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable lazy initialization of the media controls.
const base::Feature kLazyInitializeMediaControls{
    "LazyInitializeMediaControls", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables lowering the priority of the resources in iframes.
const base::Feature kLowPriorityIframes{"LowPriorityIframes",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If this feature is enabled, media-device enumerations use a cache that is
// invalidated upon notifications sent by base::SystemMonitor. If disabled, the
// cache is considered invalid on every enumeration request.
const base::Feature kMediaDevicesSystemMonitorCache {
  "MediaDevicesSystemMonitorCaching",
#if defined(OS_MACOSX) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Instead of BrowserPlugin or GuestViews, MimeHandlerView will use a cross
// process frame to render its handler.
const base::Feature kMimeHandlerViewInCrossProcessFrame{
    "MimeHandlerViewInCrossProcessFrame", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables/disables the video capture service.
const base::Feature kMojoVideoCapture {
  "MojoVideoCapture",
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// A secondary switch used in combination with kMojoVideoCapture.
// This is intended as a kill switch to allow disabling the service on
// particular groups of devices even if they forcibly enable kMojoVideoCapture
// via a command-line argument.
const base::Feature kMojoVideoCaptureSecondary{
    "MojoVideoCaptureSecondary", base::FEATURE_ENABLED_BY_DEFAULT};

// If the network service is enabled, runs it in process.
const base::Feature kNetworkServiceInProcess{"NetworkServiceInProcess",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for Web Notification content images.
const base::Feature kNotificationContentImage{"NotificationContentImage",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the notification trigger API.
const base::Feature kNotificationTriggers{"NotificationTriggers",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Origin Policy. See https://crbug.com/751996
const base::Feature kOriginPolicy{"OriginPolicy",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Origin Trials for controlling access to feature/API experiments.
const base::Feature kOriginTrials{"OriginTrials",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// History navigation in response to horizontal overscroll (aka gesture-nav).
const base::Feature kOverscrollHistoryNavigation{
  "OverscrollHistoryNavigation",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Blink PageLifecycle feature. See https://crbug.com/775194
const base::Feature kPageLifecycle{"PageLifecycle",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Whether document level event listeners should default 'passive' to true.
const base::Feature kPassiveDocumentEventListeners{
    "PassiveDocumentEventListeners", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether document level wheel and mousewheel event listeners should default
// 'passive' to true.
const base::Feature kPassiveDocumentWheelEventListeners{
    "PassiveDocumentWheelEventListeners", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether we should force a touchstart and first touchmove per scroll event
// listeners to be non-blocking during fling.
const base::Feature kPassiveEventListenersDueToFling{
    "PassiveEventListenersDueToFling", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether PaymentRequest exposes hasEnrolledInstrument API.
const base::Feature kPaymentRequestHasEnrolledInstrument = {
    "PaymentRequestHasEnrolledInstrument", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether PDF files should be rendered in diffent processes based on origin.
const base::Feature kPdfIsolation = {"PdfIsolation",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Whether we should use the navigation_client mojo interface for navigations.
const base::Feature kPerNavigationMojoInterface = {
    "PerNavigationMojoInterface", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Whether cross-site frames should get their own SiteInstance even when
// strict site isolation is disabled. These SiteInstances will still be
// grouped into a shared default process based on BrowsingInstance.
const base::Feature kProcessSharingWithStrictSiteInstances{
    "ProcessSharingWithStrictSiteInstances", base::FEATURE_DISABLED_BY_DEFAULT};

// Under this flag bootstrap (aka startup) tasks will be prioritized. This flag
// is used by various modules to determine whether special scheduling
// arrangements need to be made to prioritize certain tasks.
const base::Feature kPrioritizeBootstrapTasks = {
    "PrioritizeBootstrapTasks", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enable raster-inducing scroll.
const base::Feature kRasterInducingScroll{"RasterInducingScroll",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// RenderDocument:
//
// Currently, a RenderFrameHost represents neither a frame nor a document, but a
// frame in a given process. A new one is created after a different-process
// navigation. The goal of RenderDocument is to get a new one for each document
// instead.

// Enable using the RenderDocument on main frame navigations.
const base::Feature kRenderDocumentForMainFrame{
    "RenderDocumentForMainFrame", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable using the RenderDocument on subframe navigations.
const base::Feature kRenderDocumentForSubframe{
    "RenderDocumentForSubframe", base::FEATURE_DISABLED_BY_DEFAULT};

// Throttle Blink's rendering pipeline based on frame visibility.
const base::Feature kRenderingPipelineThrottling{
    "RenderingPipelineThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables resampling input events on main thread.
const base::Feature kResamplingInputEvents{"ResamplingInputEvents",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Loading Dispatcher v0 support with ResourceLoadScheduler (crbug.com/729954).
const base::Feature kResourceLoadScheduler{"ResourceLoadScheduler",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Run video capture service in the Browser process as opposed to a dedicated
// utility process
const base::Feature kRunVideoCaptureServiceInBrowserProcess{
    "RunVideoCaptureServiceInBrowserProcess",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Save the scroll anchor and use it to restore scroll position.
const base::Feature kScrollAnchorSerialization{
    "ScrollAnchorSerialization", base::FEATURE_ENABLED_BY_DEFAULT};

// Make sendBeacon throw for a Blob with a non simple type.
const base::Feature kSendBeaconThrowForBlobWithNonSimpleType{
    "SendBeaconThrowForBlobWithNonSimpleType",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSecMetadata{"SecMetadata",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Keep processes with service workers controlling clients from other
// processes at foreground priority. (crbug.com/928904)
const base::Feature kServiceWorkerForegroundPriority{
    "ServiceWorkerForegroundPriority", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables long running message dispatch for service workers.
// This is a temporary addition only to be used for the Android Messages
// integration with ChromeOS (http://crbug.com/823256).
const base::Feature kServiceWorkerLongRunningMessage{
    "ServiceWorkerLongRunningMessage", base::FEATURE_ENABLED_BY_DEFAULT};

// Service worker based payment apps as defined by w3c here:
// https://w3c.github.io/webpayments-payment-apps-api/
// TODO(rouslan): Remove this.
const base::Feature kServiceWorkerPaymentApps{"ServiceWorkerPaymentApps",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// http://tc39.github.io/ecmascript_sharedmem/shmem.html
const base::Feature kSharedArrayBuffer {
  "SharedArrayBuffer",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Signed Exchange Reporting for distributors
// https://www.chromestatus.com/features/5687904902840320
const base::Feature kSignedExchangeReportingForDistributors{
    "SignedExchangeReportingForDistributors", base::FEATURE_ENABLED_BY_DEFAULT};

// Origin-Signed HTTP Exchanges (for WebPackage Loading)
// https://www.chromestatus.com/features/5745285984681984
const base::Feature kSignedHTTPExchange{"SignedHTTPExchange",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether SpareRenderProcessHostManager tries to always have a warm
// spare renderer process around for the most recently requested BrowserContext.
// This feature is only consulted in site-per-process mode.
const base::Feature kSpareRendererForSitePerProcess{
    "SpareRendererForSitePerProcess", base::FEATURE_ENABLED_BY_DEFAULT};

// Dispatch touch events to "SyntheticGestureController" for events from
// Devtool Protocol Input.dispatchTouchEvent to simulate touch events close to
// real OS events.
const base::Feature kSyntheticPointerActions{"SyntheticPointerActions",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Throttle Blink timers in out-of-view cross origin frames.
const base::Feature kTimerThrottlingForHiddenFrames{
    "TimerThrottlingForHiddenFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables async touchpad pinch zoom events. We check the ACK of the first
// synthetic wheel event in a pinch sequence, then send the rest of the
// synthetic wheel events of the pinch sequence as non-blocking if the first
// event’s ACK is not canceled.
const base::Feature kTouchpadAsyncPinchEvents{"TouchpadAsyncPinchEvents",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Skips the browser touch event filter, ensuring that events that reach the
// queue and would otherwise be filtered out will instead be passed onto the
// renderer compositor process as long as the page hasn't timed out. Which event
// types will be always forwarded is controlled by the "type" FeatureParam,
// which can be either "discrete" (default) or "all".
const base::Feature kSkipBrowserTouchFilter{"SkipBrowserTouchFilter",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const char kSkipBrowserTouchFilterTypeParamName[] = "type";
const char kSkipBrowserTouchFilterTypeParamValueDiscrete[] = "discrete";
const char kSkipBrowserTouchFilterTypeParamValueAll[] = "all";

// Allows developers transfer user activation state to any target window in the
// frame tree.
const base::Feature kUserActivationPostMessageTransfer{
    "UserActivationPostMessageTransfer", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows user activation propagation to all frames having the same origin as
// the activation notifier frame.  This is an intermediate measure before we
// have an iframe attribute to declaratively allow user activation propagation
// to subframes.
const base::Feature kUserActivationSameOriginVisibility{
    "UserActivationSameOriginVisibility", base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental simple user-activation model where the user gesture state is
// tracked through a frame-based state instead of the gesture tokens we use
// today.
const base::Feature kUserActivationV2{"UserActivationV2",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental replacement for the `User-Agent` header, defined in
// https://tools.ietf.org/html/draft-west-ua-client-hints.
const base::Feature kUserAgentClientHint{"UserAgentClientHint",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables V8's low memory mode for subframes. This is used only
// in conjunction with the --site-per-process feature.
const base::Feature kV8LowMemoryModeForSubframes{
    "V8LowMemoryModeForSubframes", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables to use the V8 Orinoco garbage collector.
const base::Feature kV8Orinoco{"V8Orinoco", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables future V8 VM features
const base::Feature kV8VmFuture{"V8VmFuture",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebAssembly structured cloning.
// http://webassembly.org/
const base::Feature kWebAssembly{"WebAssembly",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Enable WebAssembly baseline compilation and tier up.
const base::Feature kWebAssemblyBaseline{"WebAssemblyBaseline",
#ifdef ARCH_CPU_X86_FAMILY
                                         base::FEATURE_ENABLED_BY_DEFAULT
#else
                                         base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable WebAssembly threads.
// https://github.com/WebAssembly/threads
const base::Feature kWebAssemblyThreads {
  "WebAssemblyThreads",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enable WebAssembly trap handler.
#if (defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MACOSX)) && \
    defined(ARCH_CPU_X86_64)
const base::Feature kWebAssemblyTrapHandler{"WebAssemblyTrapHandler",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kWebAssemblyTrapHandler{"WebAssemblyTrapHandler",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Controls whether the visibility of a WebContents can be OCCLUDED. When
// disabled, an occluded WebContents behaves exactly like a VISIBLE WebContents.
const base::Feature kWebContentsOcclusion {
  "WebContentsOcclusion",
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Controls whether the WebAuthentication API is enabled:
// https://w3c.github.io/webauthn
const base::Feature kWebAuth{"WebAuthentication",
                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether BLE authenticators can be used via the WebAuthentication
// API. https://w3c.github.io/webauthn
const base::Feature kWebAuthBle{"WebAuthenticationBle",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether CTAP2 devices can communicate via the WebAuthentication API
// using pairingless BLE protocol.
// https://w3c.github.io/webauthn
const base::Feature kWebAuthCable {
  "WebAuthenticationCable",
#if defined(OS_LINUX)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// If WebGL Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kWebGLImageChromium{"WebGLImageChromium",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enable experimental policy-controlled features and LAPIs
const base::Feature kExperimentalProductivityFeatures{
    "ExperimentalProductivityFeatures", base::FEATURE_DISABLED_BY_DEFAULT};

// The JavaScript API for payments on the web.
// TODO(rouslan): Remove this.
const base::Feature kWebPayments{"WebPayments",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Makes WebRTC use ECDSA certs by default (i.e., when no cert type was
// specified in JS).
const base::Feature kWebRtcEcdsaDefault{"WebRTC-EnableWebRtcEcdsa",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables HW H264 encoding on Android.
const base::Feature kWebRtcHWH264Encoding{"WebRtcHWH264Encoding",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables HW VP8 encoding on Android.
const base::Feature kWebRtcHWVP8Encoding {
  "WebRtcHWVP8Encoding",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enables HW VP9 encoding on Android.
const base::Feature kWebRtcHWVP9Encoding {
  "WebRtcHWVP9Encoding",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables negotiation of experimental multiplex codec in SDP.
const base::Feature kWebRtcMultiplexCodec{"WebRTC-MultiplexCodec",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Fallback from hardware encoder (if available) to software, for WebRTC
// screensharing that uses temporal scalability.
const base::Feature kWebRtcScreenshareSwEncoding{
    "WebRtcScreenshareSwEncoding", base::FEATURE_DISABLED_BY_DEFAULT};

// Use GpuMemoryBuffer backed VideoFrames in media streams.
const base::Feature kWebRtcUseGpuMemoryBufferVideoFrames{
    "WebRTC-UseGpuMemoryBufferVideoFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Causes WebRTC to replace host ICE candidate IP addresses with generated
// names ending in ".local" and resolve them using mDNS.
// http://crbug.com/878465
const base::Feature kWebRtcHideLocalIpsWithMdns{
    "WebRtcHideLocalIpsWithMdns", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
const base::Feature kWebUsb{"WebUSB", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the WebXR Device API is enabled.
const base::Feature kWebXr{"WebXR", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables access to raycasting against estimated XR scene geometry.
const base::Feature kWebXrHitTest{"WebXRHitTest",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Wipe corrupt v2 IndexedDB databases.
const base::Feature kWipeCorruptV2IDBDatabases{
    "WipeCorruptV2IDBDatabases", base::FEATURE_ENABLED_BY_DEFAULT};

// Start streaming scripts on script preload.
const base::Feature kScriptStreamingOnPreload{
    "ScriptStreamingOnPreload", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Autofill Accessibility in Android.
// crbug.com/627860
const base::Feature kAndroidAutofillAccessibility{
    "AndroidAutofillAccessibility", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables hiding incorrectly-sized frames while in fullscreen.
const base::Feature kHideIncorrectlySizedFullscreenFrames{
    "HideIncorrectlySizedFullscreenFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Sets moderate binding to background renderers playing media, when enabled.
// Else the renderer will have strong binding.
const base::Feature kBackgroundMediaRendererHasModerateBinding{
    "BackgroundMediaRendererHasModerateBinding",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebNFC API is enabled:
// https://w3c.github.io/web-nfc/
const base::Feature kWebNfc{"WebNFC", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
// Makes all WebUI that uses Polymer use 2.x version.
// TODO(dpapad): Remove this once Polymer 2 migration is done,
// https://crbug.com/738611.
const base::Feature kWebUIPolymer2{"WebUIPolymer2",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// If this flag is enabled, Web UI pages can call DisablePolymer2() on the
// shared resource during setup in order to use Polymer 1, regardless of the
// state of the WebUIPolymer2 flag. Note: Currently, this only supports one Web
// UI page disabling Polymer 2.
// TODO(dpapad): Remove this once Polymer 2 migration is done,
// https://crbug.com/738611.
const base::Feature kWebUIPolymer2Exceptions{"WebUIPolymer2Exceptions",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// Enables caching of media devices for the purpose of enumerating them.
const base::Feature kDeviceMonitorMac{"DeviceMonitorMac",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enable IOSurface based screen capturer.
const base::Feature kIOSurfaceCapturer{"IOSurfaceCapturer",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMacV2GPUSandbox{"MacV2GPUSandbox",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables retrying to obtain list of available cameras on Macbooks after
// restarting the video capture service if a previous attempt delivered zero
// cameras.
const base::Feature kRetryGetVideoCaptureDeviceInfos{
    "RetryGetVideoCaptureDeviceInfos", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

#if defined(WEBRTC_USE_PIPEWIRE)
// Controls whether the PipeWire support for screen capturing is enabled on the
// Wayland display server.
const base::Feature kWebRtcPipeWireCapturer{"WebRTCPipeWireCapturer",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(WEBRTC_USE_PIPEWIRE)

enum class VideoCaptureServiceConfiguration {
  kEnabledForOutOfProcess,
  kEnabledForBrowserProcess,
  kDisabled
};

bool ShouldEnableVideoCaptureService() {
  return base::FeatureList::IsEnabled(features::kMojoVideoCapture) &&
         base::FeatureList::IsEnabled(features::kMojoVideoCaptureSecondary);
}

VideoCaptureServiceConfiguration GetVideoCaptureServiceConfiguration() {
  if (!ShouldEnableVideoCaptureService())
    return VideoCaptureServiceConfiguration::kDisabled;

#if defined(OS_ANDROID)
  return VideoCaptureServiceConfiguration::kEnabledForBrowserProcess;
#elif defined(OS_CHROMEOS)
  return media::ShouldUseCrosCameraService()
             ? VideoCaptureServiceConfiguration::kEnabledForBrowserProcess
             : VideoCaptureServiceConfiguration::kEnabledForOutOfProcess;
#else
  return base::FeatureList::IsEnabled(
             features::kRunVideoCaptureServiceInBrowserProcess)
             ? VideoCaptureServiceConfiguration::kEnabledForBrowserProcess
             : VideoCaptureServiceConfiguration::kEnabledForOutOfProcess;
#endif
}

bool IsVideoCaptureServiceEnabledForOutOfProcess() {
  return GetVideoCaptureServiceConfiguration() ==
         VideoCaptureServiceConfiguration::kEnabledForOutOfProcess;
}

bool IsVideoCaptureServiceEnabledForBrowserProcess() {
  return GetVideoCaptureServiceConfiguration() ==
         VideoCaptureServiceConfiguration::kEnabledForBrowserProcess;
}

}  // namespace features
