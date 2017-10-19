// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"
#include "services/device/public/cpp/device_features.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"

using blink::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID)
  // Android does not have support for PagePopup
  WebRuntimeFeatures::EnablePagePopup(false);
  // No plan to support complex UI for date/time INPUT types.
  WebRuntimeFeatures::EnableInputMultipleFieldsUI(false);
  // Android does not yet support SharedWorker. crbug.com/154571
  WebRuntimeFeatures::EnableSharedWorker(false);
  // Android does not yet support NavigatorContentUtils.
  WebRuntimeFeatures::EnableNavigatorContentUtils(false);
  WebRuntimeFeatures::EnableOrientationEvent(true);
  WebRuntimeFeatures::EnableFastMobileScrolling(true);
  WebRuntimeFeatures::EnableMediaCapture(true);
  // Android won't be able to reliably support non-persistent notifications, the
  // intended behavior for which is in flux by itself.
  WebRuntimeFeatures::EnableNotificationConstructor(false);
  // Android does not yet support switching of audio output devices
  WebRuntimeFeatures::EnableAudioOutputDevices(false);
  WebRuntimeFeatures::EnableAutoplayMutedVideos(true);
  // Android does not yet support SystemMonitor.
  WebRuntimeFeatures::EnableOnDeviceChange(false);
  WebRuntimeFeatures::EnableMediaSession(true);
  WebRuntimeFeatures::EnableMediaControlsOverlayPlayButton(true);
  WebRuntimeFeatures::EnableRemotePlaybackBackend(true);
#else  // defined(OS_ANDROID)
  WebRuntimeFeatures::EnableNavigatorContentUtils(true);
  // Tracking bug for the implementation: https://crbug.com/728609
  WebRuntimeFeatures::EnableRemotePlaybackBackend(false);
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(USE_AURA)
  WebRuntimeFeatures::EnableCompositedSelectionUpdate(true);
#endif

#if !(defined OS_ANDROID || defined OS_CHROMEOS)
  // Only Android, ChromeOS support NetInfo downlinkMax, type and ontypechange
  // now.
  WebRuntimeFeatures::EnableNetInfoDownlinkMax(false);
#endif

// Web Bluetooth is shipped on Android, ChromeOS & MacOS, experimental
// otherwise.
#if defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_MACOSX)
  WebRuntimeFeatures::EnableWebBluetooth(true);
#endif

#if defined(OS_CHROMEOS)
  WebRuntimeFeatures::EnableForceTallerSelectPopup(true);
#endif

// The Notification Center on Mac OS X does not support content images.
#if !defined(OS_MACOSX)
  WebRuntimeFeatures::EnableNotificationContentImage(true);
#endif
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  bool enableExperimentalWebPlatformFeatures = command_line.HasSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  if (enableExperimentalWebPlatformFeatures)
    WebRuntimeFeatures::EnableExperimentalFeatures(true);

  WebRuntimeFeatures::EnableOriginTrials(
      base::FeatureList::IsEnabled(features::kOriginTrials));

  WebRuntimeFeatures::EnableFeaturePolicy(
      base::FeatureList::IsEnabled(features::kFeaturePolicy));

  if (!base::FeatureList::IsEnabled(features::kWebUsb))
    WebRuntimeFeatures::EnableWebUsb(false);

  SetRuntimeFeatureDefaultsForPlatform();

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::EnableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableNotifications)) {
    WebRuntimeFeatures::EnableNotifications(false);

    // Chrome's Push Messaging implementation relies on Web Notifications.
    WebRuntimeFeatures::EnablePushMessaging(false);
  }

  if (!base::FeatureList::IsEnabled(features::kNotificationContentImage))
    WebRuntimeFeatures::EnableNotificationContentImage(false);

  WebRuntimeFeatures::EnableWebAssemblyStreaming(
      base::FeatureList::IsEnabled(features::kWebAssemblyStreaming));

  if (command_line.HasSwitch(switches::kDisableSharedWorkers))
    WebRuntimeFeatures::EnableSharedWorker(false);

  if (command_line.HasSwitch(switches::kDisableSpeechAPI))
    WebRuntimeFeatures::EnableScriptedSpeech(false);

  if (command_line.HasSwitch(switches::kDisableFileSystem))
    WebRuntimeFeatures::EnableFileSystem(false);

  if (command_line.HasSwitch(switches::kEnableExperimentalCanvasFeatures))
    WebRuntimeFeatures::EnableExperimentalCanvasFeatures(true);

  if (!command_line.HasSwitch(switches::kDisableAcceleratedJpegDecoding))
    WebRuntimeFeatures::EnableDecodeToYUV(true);

  if (command_line.HasSwitch(switches::kEnableDisplayList2dCanvas))
    WebRuntimeFeatures::EnableDisplayList2dCanvas(true);

  if (command_line.HasSwitch(switches::kDisableDisplayList2dCanvas))
    WebRuntimeFeatures::EnableDisplayList2dCanvas(false);

  if (command_line.HasSwitch(switches::kForceDisplayList2dCanvas))
    WebRuntimeFeatures::ForceDisplayList2dCanvas(true);

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::EnableWebGLDraftExtensions(true);

  if (command_line.HasSwitch(switches::kEnableAutomation) ||
      command_line.HasSwitch(switches::kHeadless)) {
    WebRuntimeFeatures::EnableAutomationControlled(true);
  }

#if defined(OS_MACOSX)
  bool enable_canvas_2d_image_chromium = command_line.HasSwitch(
      switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisable2dCanvasImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu);

  if (enable_canvas_2d_image_chromium) {
    enable_canvas_2d_image_chromium =
        base::FeatureList::IsEnabled(features::kCanvas2DImageChromium);
  }
#else
  bool enable_canvas_2d_image_chromium = false;
#endif
  WebRuntimeFeatures::EnableCanvas2dImageChromium(
      enable_canvas_2d_image_chromium);

#if defined(OS_MACOSX)
  bool enable_web_gl_image_chromium = command_line.HasSwitch(
      switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisableWebGLImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu);

  if (enable_web_gl_image_chromium) {
    enable_web_gl_image_chromium =
        base::FeatureList::IsEnabled(features::kWebGLImageChromium);
  }
#else
  bool enable_web_gl_image_chromium =
      command_line.HasSwitch(switches::kEnableWebGLImageChromium);
#endif
  WebRuntimeFeatures::EnableWebGLImageChromium(enable_web_gl_image_chromium);

  if (command_line.HasSwitch(switches::kForceOverlayFullscreenVideo))
    WebRuntimeFeatures::ForceOverlayFullscreenVideo(true);

  if (ui::IsOverlayScrollbarEnabled())
    WebRuntimeFeatures::EnableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnablePreciseMemoryInfo))
    WebRuntimeFeatures::EnablePreciseMemoryInfo(true);

  if (command_line.HasSwitch(switches::kEnablePrintBrowser))
    WebRuntimeFeatures::EnablePrintBrowser(true);

  if (command_line.HasSwitch(switches::kEnableNetworkInformationDownlinkMax) ||
      enableExperimentalWebPlatformFeatures) {
    WebRuntimeFeatures::EnableNetInfoDownlinkMax(true);
  }

  if (command_line.HasSwitch(switches::kReducedReferrerGranularity))
    WebRuntimeFeatures::EnableReducedReferrerGranularity(true);

  if (command_line.HasSwitch(switches::kRootLayerScrolls))
    WebRuntimeFeatures::EnableRootLayerScrolling(true);

  if (command_line.HasSwitch(switches::kDisablePermissionsAPI))
    WebRuntimeFeatures::EnablePermissionsAPI(false);

  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::EnableV8IdleTasks(false);
  else
    WebRuntimeFeatures::EnableV8IdleTasks(true);

  if (command_line.HasSwitch(switches::kEnableWebVR))
    WebRuntimeFeatures::EnableWebVR(true);

  WebRuntimeFeatures::EnableWebVRExperimentalRendering(
      base::FeatureList::IsEnabled(features::kWebVRExperimentalRendering));

  if (command_line.HasSwitch(switches::kDisablePresentationAPI))
    WebRuntimeFeatures::EnablePresentationAPI(false);

  if (command_line.HasSwitch(switches::kDisableRemotePlaybackAPI))
    WebRuntimeFeatures::EnableRemotePlaybackAPI(false);

  WebRuntimeFeatures::EnableScrollAnchoring(
      base::FeatureList::IsEnabled(features::kScrollAnchoring) ||
      enableExperimentalWebPlatformFeatures);

  if (command_line.HasSwitch(switches::kEnableSlimmingPaintV175))
    WebRuntimeFeatures::EnableFeatureFromString("SlimmingPaintV175", true);
  if (command_line.HasSwitch(switches::kEnableSlimmingPaintV2))
    WebRuntimeFeatures::EnableSlimmingPaintV2(true);

  if (base::FeatureList::IsEnabled(features::kLazyParseCSS))
    WebRuntimeFeatures::EnableLazyParseCSS(true);

  WebRuntimeFeatures::EnablePassiveDocumentEventListeners(
      base::FeatureList::IsEnabled(features::kPassiveDocumentEventListeners));

  WebRuntimeFeatures::EnableFeatureFromString(
      "FontCacheScaling",
      base::FeatureList::IsEnabled(features::kFontCacheScaling));

  WebRuntimeFeatures::EnableFeatureFromString(
      "FramebustingNeedsSameOriginOrUserGesture",
      base::FeatureList::IsEnabled(
          features::kFramebustingNeedsSameOriginOrUserGesture));

  WebRuntimeFeatures::EnableFeatureFromString(
      "VibrateRequiresUserGesture",
      base::FeatureList::IsEnabled(features::kVibrateRequiresUserGesture));

  if (command_line.HasSwitch(switches::kDisableBackgroundTimerThrottling))
    WebRuntimeFeatures::EnableTimerThrottlingForBackgroundTabs(false);

  WebRuntimeFeatures::EnableExpensiveBackgroundTimerThrottling(
      base::FeatureList::IsEnabled(
          features::kExpensiveBackgroundTimerThrottling));

  if (base::FeatureList::IsEnabled(features::kHeapCompaction))
    WebRuntimeFeatures::EnableHeapCompaction(true);

  WebRuntimeFeatures::EnableRenderingPipelineThrottling(
      base::FeatureList::IsEnabled(features::kRenderingPipelineThrottling));

  WebRuntimeFeatures::EnableTimerThrottlingForHiddenFrames(
      base::FeatureList::IsEnabled(features::kTimerThrottlingForHiddenFrames));

  WebRuntimeFeatures::EnableTouchpadAndWheelScrollLatching(
      base::FeatureList::IsEnabled(features::kTouchpadAndWheelScrollLatching));

  if (base::FeatureList::IsEnabled(
          features::kSendBeaconThrowForBlobWithNonSimpleType))
    WebRuntimeFeatures::EnableSendBeaconThrowForBlobWithNonSimpleType(true);

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableMediaSessionAPI))
    WebRuntimeFeatures::EnableMediaSession(false);
#endif

  WebRuntimeFeatures::EnablePaymentRequest(
      base::FeatureList::IsEnabled(features::kWebPayments));

  WebRuntimeFeatures::EnableServiceWorkerScriptStreaming(
      base::FeatureList::IsEnabled(features::kServiceWorkerScriptStreaming));

  WebRuntimeFeatures::EnableOffMainThreadFetch(
      base::FeatureList::IsEnabled(features::kOffMainThreadFetch));

  WebRuntimeFeatures::EnableMojoBlobs(features::IsMojoBlobsEnabled());

  WebRuntimeFeatures::EnableNetworkService(
      base::FeatureList::IsEnabled(features::kNetworkService));

  if (base::FeatureList::IsEnabled(features::kGamepadExtensions))
    WebRuntimeFeatures::EnableGamepadExtensions(true);

  if (base::FeatureList::IsEnabled(features::kCompositeOpaqueFixedPosition))
    WebRuntimeFeatures::EnableFeatureFromString("CompositeOpaqueFixedPosition",
                                                true);

  if (!base::FeatureList::IsEnabled(features::kCompositeOpaqueScrollers))
    WebRuntimeFeatures::EnableFeatureFromString("CompositeOpaqueScrollers",
                                                false);
  if (base::FeatureList::IsEnabled(features::kCompositorTouchAction))
    WebRuntimeFeatures::EnableCompositorTouchAction(true);

  if (base::FeatureList::IsEnabled(features::kGenericSensor)) {
    WebRuntimeFeatures::EnableGenericSensor(true);
    if (base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses))
      WebRuntimeFeatures::EnableGenericSensorExtraClasses(true);
  }

  if (base::FeatureList::IsEnabled(features::kLoadingWithMojo) ||
      base::FeatureList::IsEnabled(features::kNetworkService))
    WebRuntimeFeatures::EnableLoadingWithMojo(true);

  if (base::FeatureList::IsEnabled(features::kOutOfBlinkCORS))
    WebRuntimeFeatures::EnableOutOfBlinkCORS(true);

  if (base::FeatureList::IsEnabled(features::kOriginManifest))
    WebRuntimeFeatures::EnableOriginManifest(true);

  WebRuntimeFeatures::EnableMediaCastOverlayButton(
      base::FeatureList::IsEnabled(media::kMediaCastOverlayButton));

  if (!base::FeatureList::IsEnabled(features::kBlockCredentialedSubresources)) {
    WebRuntimeFeatures::EnableFeatureFromString("BlockCredentialedSubresources",
                                                false);
  }

  WebRuntimeFeatures::EnableFeatureFromString(
      "AllowContentInitiatedDataUrlNavigations",
      base::FeatureList::IsEnabled(
          features::kAllowContentInitiatedDataUrlNavigations));

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebNfc))
    WebRuntimeFeatures::EnableWebNfc(true);
#endif

  if (media::GetEffectiveAutoplayPolicy(command_line) !=
      switches::autoplay::kNoUserGestureRequiredPolicy) {
    WebRuntimeFeatures::EnableAutoplayMutedVideos(true);
  }

  if (!base::FeatureList::IsEnabled(features::kWebAuth) &&
      !enableExperimentalWebPlatformFeatures)
    WebRuntimeFeatures::EnableWebAuth(false);

  WebRuntimeFeatures::EnableModuleScripts(
      base::FeatureList::IsEnabled(features::kModuleScripts));

  WebRuntimeFeatures::EnableClientPlaceholdersForServerLoFi(
      base::GetFieldTrialParamValue("PreviewsClientLoFi",
                                    "replace_server_placeholders") == "true");

  WebRuntimeFeatures::EnableResourceLoadScheduler(
      base::FeatureList::IsEnabled(features::kResourceLoadScheduler));

  if (command_line.HasSwitch(
          switches::kDisableOriginTrialControlledBlinkFeatures)) {
    WebRuntimeFeatures::EnableOriginTrialControlledFeatures(false);
  }

  WebRuntimeFeatures::EnableLazyInitializeMediaControls(
      base::FeatureList::IsEnabled(features::kLazyInitializeMediaControls));

  WebRuntimeFeatures::EnableMediaEngagementBypassAutoplayPolicies(
      base::FeatureList::IsEnabled(
          media::kMediaEngagementBypassAutoplayPolicies));

  WebRuntimeFeatures::EnableModuleScriptsDynamicImport(
      base::FeatureList::IsEnabled(features::kModuleScriptsDynamicImport));

  WebRuntimeFeatures::EnableOverflowIconsForMediaControls(
      base::FeatureList::IsEnabled(media::kOverflowIconsForMediaControls));

  WebRuntimeFeatures::EnableAllowActivationDelegationAttr(
      base::FeatureList::IsEnabled(features::kAllowActivationDelegationAttr));

  WebRuntimeFeatures::EnableModernMediaControls(
      base::FeatureList::IsEnabled(media::kUseModernMediaControls));

  WebRuntimeFeatures::EnableWorkStealingInScriptRunner(
      base::FeatureList::IsEnabled(features::kWorkStealingInScriptRunner));

  // Enable explicitly enabled features, and then disable explicitly disabled
  // ones.
  for (const std::string& feature :
       FeaturesFromSwitch(command_line, switches::kEnableBlinkFeatures)) {
    WebRuntimeFeatures::EnableFeatureFromString(feature, true);
  }
  for (const std::string& feature :
       FeaturesFromSwitch(command_line, switches::kDisableBlinkFeatures)) {
    WebRuntimeFeatures::EnableFeatureFromString(feature, false);
  }

  if (base::FeatureList::IsEnabled(features::kV8ContextSnapshot))
    WebRuntimeFeatures::EnableV8ContextSnapshot(true);
};

}  // namespace content
