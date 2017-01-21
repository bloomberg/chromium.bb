// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_switches.h"

using blink::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID)
  // Android does not have support for PagePopup
  WebRuntimeFeatures::enablePagePopup(false);
  // No plan to support complex UI for date/time INPUT types.
  WebRuntimeFeatures::enableInputMultipleFieldsUI(false);
  // Android does not yet support SharedWorker. crbug.com/154571
  WebRuntimeFeatures::enableSharedWorker(false);
  // Android does not yet support NavigatorContentUtils.
  WebRuntimeFeatures::enableNavigatorContentUtils(false);
  WebRuntimeFeatures::enableOrientationEvent(true);
  WebRuntimeFeatures::enableFastMobileScrolling(true);
  WebRuntimeFeatures::enableMediaCapture(true);
  // Android won't be able to reliably support non-persistent notifications, the
  // intended behavior for which is in flux by itself.
  WebRuntimeFeatures::enableNotificationConstructor(false);
  // Android does not yet support switching of audio output devices
  WebRuntimeFeatures::enableAudioOutputDevices(false);
  WebRuntimeFeatures::enableAutoplayMutedVideos(true);
  // Android does not yet support SystemMonitor.
  WebRuntimeFeatures::enableOnDeviceChange(false);
  WebRuntimeFeatures::enableMediaSession(true);
#else  // defined(OS_ANDROID)
  WebRuntimeFeatures::enableNavigatorContentUtils(true);
  if (base::FeatureList::IsEnabled(
          features::kCrossOriginMediaPlaybackRequiresUserGesture)) {
    WebRuntimeFeatures::enableAutoplayMutedVideos(true);
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(USE_AURA)
  WebRuntimeFeatures::enableCompositedSelectionUpdate(true);
#endif

#if !(defined OS_ANDROID || defined OS_CHROMEOS)
    // Only Android, ChromeOS support NetInfo right now.
    WebRuntimeFeatures::enableNetworkInformation(false);
#endif

// Web Bluetooth is shipped on Android, ChromeOS & MacOS, experimental
// otherwise.
#if defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_MACOSX)
  WebRuntimeFeatures::enableWebBluetooth(true);
#endif
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  bool enableExperimentalWebPlatformFeatures = command_line.HasSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  if (enableExperimentalWebPlatformFeatures)
    WebRuntimeFeatures::enableExperimentalFeatures(true);

  WebRuntimeFeatures::enableOriginTrials(
      base::FeatureList::IsEnabled(features::kOriginTrials));

  WebRuntimeFeatures::enableFeaturePolicy(
      base::FeatureList::IsEnabled(features::kFeaturePolicy));

  if (!base::FeatureList::IsEnabled(features::kWebUsb))
    WebRuntimeFeatures::enableWebUsb(false);

  SetRuntimeFeatureDefaultsForPlatform();

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::enableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableNotifications)) {
    WebRuntimeFeatures::enableNotifications(false);

    // Chrome's Push Messaging implementation relies on Web Notifications.
    WebRuntimeFeatures::enablePushMessaging(false);
  }

  if (!base::FeatureList::IsEnabled(features::kNotificationContentImage))
    WebRuntimeFeatures::enableNotificationContentImage(false);

  // For the time being, wasm serialization is separately controlled
  // by this flag. WebAssembly APIs and compilation is now enabled
  // unconditionally in V8.
  if (base::FeatureList::IsEnabled(features::kWebAssembly))
    WebRuntimeFeatures::enableWebAssemblySerialization(true);

  WebRuntimeFeatures::enableSharedArrayBuffer(
      base::FeatureList::IsEnabled(features::kSharedArrayBuffer));

  if (command_line.HasSwitch(switches::kDisableSharedWorkers))
    WebRuntimeFeatures::enableSharedWorker(false);

  if (command_line.HasSwitch(switches::kDisableSpeechAPI))
    WebRuntimeFeatures::enableScriptedSpeech(false);

  if (command_line.HasSwitch(switches::kDisableFileSystem))
    WebRuntimeFeatures::enableFileSystem(false);

  if (command_line.HasSwitch(switches::kEnableExperimentalCanvasFeatures))
    WebRuntimeFeatures::enableExperimentalCanvasFeatures(true);

  if (!command_line.HasSwitch(switches::kDisableAcceleratedJpegDecoding))
    WebRuntimeFeatures::enableDecodeToYUV(true);

  if (command_line.HasSwitch(switches::kEnableDisplayList2dCanvas))
    WebRuntimeFeatures::enableDisplayList2dCanvas(true);

  if (command_line.HasSwitch(switches::kDisableDisplayList2dCanvas))
    WebRuntimeFeatures::enableDisplayList2dCanvas(false);

  if (command_line.HasSwitch(switches::kForceDisplayList2dCanvas))
    WebRuntimeFeatures::forceDisplayList2dCanvas(true);

  if (command_line.HasSwitch(
      switches::kEnableCanvas2dDynamicRenderingModeSwitching))
    WebRuntimeFeatures::enableCanvas2dDynamicRenderingModeSwitching(true);

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::enableWebGLDraftExtensions(true);

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
  WebRuntimeFeatures::enableCanvas2dImageChromium(
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
  WebRuntimeFeatures::enableWebGLImageChromium(enable_web_gl_image_chromium);

  if (command_line.HasSwitch(switches::kForceOverlayFullscreenVideo))
    WebRuntimeFeatures::forceOverlayFullscreenVideo(true);

  if (ui::IsOverlayScrollbarEnabled())
    WebRuntimeFeatures::enableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnablePreciseMemoryInfo))
    WebRuntimeFeatures::enablePreciseMemoryInfo(true);

  if (command_line.HasSwitch(switches::kEnableNetworkInformation) ||
      enableExperimentalWebPlatformFeatures) {
    WebRuntimeFeatures::enableNetworkInformation(true);
  }

  if (!base::FeatureList::IsEnabled(features::kCredentialManagementAPI))
    WebRuntimeFeatures::enableCredentialManagerAPI(false);

  if (command_line.HasSwitch(switches::kReducedReferrerGranularity))
    WebRuntimeFeatures::enableReducedReferrerGranularity(true);

  if (command_line.HasSwitch(switches::kRootLayerScrolls))
    WebRuntimeFeatures::enableRootLayerScrolling(true);

  if (command_line.HasSwitch(switches::kDisablePermissionsAPI))
    WebRuntimeFeatures::enablePermissionsAPI(false);

  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::enableV8IdleTasks(false);
  else
    WebRuntimeFeatures::enableV8IdleTasks(true);

  if (command_line.HasSwitch(switches::kEnableWebVR))
    WebRuntimeFeatures::enableWebVR(true);

  if (command_line.HasSwitch(switches::kDisablePresentationAPI))
    WebRuntimeFeatures::enablePresentationAPI(false);

  if (command_line.HasSwitch(switches::kDisableRemotePlaybackAPI))
    WebRuntimeFeatures::enableRemotePlaybackAPI(false);

  const std::string webfonts_intervention_v2_group_name =
      base::FieldTrialList::FindFullName("WebFontsInterventionV2");
  const std::string webfonts_intervention_v2_about_flag =
      command_line.GetSwitchValueASCII(switches::kEnableWebFontsInterventionV2);
  if (!webfonts_intervention_v2_about_flag.empty()) {
    WebRuntimeFeatures::enableWebFontsInterventionV2With2G(
        webfonts_intervention_v2_about_flag.compare(
            switches::kEnableWebFontsInterventionV2SwitchValueEnabledWith2G) ==
        0);
    WebRuntimeFeatures::enableWebFontsInterventionV2With3G(
        webfonts_intervention_v2_about_flag.compare(
            switches::kEnableWebFontsInterventionV2SwitchValueEnabledWith3G) ==
        0);
    WebRuntimeFeatures::enableWebFontsInterventionV2WithSlow2G(
        webfonts_intervention_v2_about_flag.compare(
            switches::
                kEnableWebFontsInterventionV2SwitchValueEnabledWithSlow2G) ==
        0);
  } else {
    WebRuntimeFeatures::enableWebFontsInterventionV2With2G(base::StartsWith(
        webfonts_intervention_v2_group_name,
        switches::kEnableWebFontsInterventionV2SwitchValueEnabledWith2G,
        base::CompareCase::INSENSITIVE_ASCII));
    WebRuntimeFeatures::enableWebFontsInterventionV2With3G(base::StartsWith(
        webfonts_intervention_v2_group_name,
        switches::kEnableWebFontsInterventionV2SwitchValueEnabledWith3G,
        base::CompareCase::INSENSITIVE_ASCII));
    WebRuntimeFeatures::enableWebFontsInterventionV2WithSlow2G(base::StartsWith(
        webfonts_intervention_v2_group_name,
        switches::kEnableWebFontsInterventionV2SwitchValueEnabledWithSlow2G,
        base::CompareCase::INSENSITIVE_ASCII));
  }
  if (command_line.HasSwitch(switches::kEnableWebFontsInterventionTrigger))
    WebRuntimeFeatures::enableWebFontsInterventionTrigger(true);

  WebRuntimeFeatures::enableScrollAnchoring(
      base::FeatureList::IsEnabled(features::kScrollAnchoring) ||
      enableExperimentalWebPlatformFeatures);

  if (command_line.HasSwitch(switches::kEnableSlimmingPaintV2))
    WebRuntimeFeatures::enableSlimmingPaintV2(true);

  if (base::FeatureList::IsEnabled(features::kSlimmingPaintInvalidation))
    WebRuntimeFeatures::enableSlimmingPaintInvalidation(true);

  if (command_line.HasSwitch(switches::kEnableSlimmingPaintInvalidation))
    WebRuntimeFeatures::enableSlimmingPaintInvalidation(true);

  if (command_line.HasSwitch(switches::kDisableSlimmingPaintInvalidation))
    WebRuntimeFeatures::enableSlimmingPaintInvalidation(false);

  if (base::FeatureList::IsEnabled(features::kDocumentWriteEvaluator))
    WebRuntimeFeatures::enableDocumentWriteEvaluator(true);

  if (base::FeatureList::IsEnabled(features::kLazyParseCSS))
    WebRuntimeFeatures::enableLazyParseCSS(true);

  WebRuntimeFeatures::enableMediaDocumentDownloadButton(
      base::FeatureList::IsEnabled(features::kMediaDocumentDownloadButton));

  WebRuntimeFeatures::enablePointerEvent(
      base::FeatureList::IsEnabled(features::kPointerEvents));

  WebRuntimeFeatures::enablePassiveDocumentEventListeners(
      base::FeatureList::IsEnabled(features::kPassiveDocumentEventListeners));

  WebRuntimeFeatures::enableFeatureFromString(
      "FontCacheScaling",
      base::FeatureList::IsEnabled(features::kFontCacheScaling));

  WebRuntimeFeatures::enableFeatureFromString(
      "FramebustingNeedsSameOriginOrUserGesture",
      base::FeatureList::IsEnabled(
          features::kFramebustingNeedsSameOriginOrUserGesture));

  if (base::FeatureList::IsEnabled(features::kParseHTMLOnMainThread))
    WebRuntimeFeatures::enableFeatureFromString("ParseHTMLOnMainThread", true);

  if (command_line.HasSwitch(switches::kDisableBackgroundTimerThrottling))
    WebRuntimeFeatures::enableTimerThrottlingForBackgroundTabs(false);

  WebRuntimeFeatures::enableExpensiveBackgroundTimerThrottling(
      base::FeatureList::IsEnabled(
          features::kExpensiveBackgroundTimerThrottling));

  if (base::FeatureList::IsEnabled(features::kHeapCompaction))
    WebRuntimeFeatures::enableHeapCompaction(true);

  WebRuntimeFeatures::enableRenderingPipelineThrottling(
    base::FeatureList::IsEnabled(features::kRenderingPipelineThrottling));

  WebRuntimeFeatures::enableTimerThrottlingForHiddenFrames(
      base::FeatureList::IsEnabled(features::kTimerThrottlingForHiddenFrames));

  if (base::FeatureList::IsEnabled(
          features::kSendBeaconThrowForBlobWithNonSimpleType))
    WebRuntimeFeatures::enableSendBeaconThrowForBlobWithNonSimpleType(true);

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableMediaSessionAPI))
    WebRuntimeFeatures::enableMediaSession(false);

  WebRuntimeFeatures::enablePaymentRequest(
      base::FeatureList::IsEnabled(features::kWebPayments));
#endif

  // Sets the RuntimeEnabledFeatures for Navigation Preload feature only when
  // '--enable-features' command line flag is given. While experimenting this
  // feature using Origin-Trial, this base::Feature is enabled by default in
  // content_features.cc. So FeatureList::IsEnabled() always returns true. But,
  // unless the command line explicitly enabled the feature, this feature should
  // be available only when a valid origin trial token is set. This check is
  // done by the generated code of
  // blink::OriginTrials::serviceWorkerNavigationPreloadEnabled(). See the
  // comments in service_worker_version.h for the details.
  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kServiceWorkerNavigationPreload.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    WebRuntimeFeatures::enableServiceWorkerNavigationPreload(true);
  }

  if (base::FeatureList::IsEnabled(features::kSpeculativeLaunchServiceWorker))
    WebRuntimeFeatures::enableSpeculativeLaunchServiceWorker(true);

  if (base::FeatureList::IsEnabled(features::kGamepadExtensions))
    WebRuntimeFeatures::enableGamepadExtensions(true);

  if (base::FeatureList::IsEnabled(features::kCompositeOpaqueFixedPosition))
    WebRuntimeFeatures::enableFeatureFromString("CompositeOpaqueFixedPosition",
        true);

  if (!base::FeatureList::IsEnabled(features::kCompositeOpaqueScrollers))
    WebRuntimeFeatures::enableFeatureFromString("CompositeOpaqueScrollers",
        false);

  if (base::FeatureList::IsEnabled(features::kGenericSensor))
    WebRuntimeFeatures::enableGenericSensor(true);

  if (base::FeatureList::IsEnabled(features::kFasterLocationReload))
    WebRuntimeFeatures::enableFasterLocationReload(true);

  // Enable features which VrShell depends on.
  if (base::FeatureList::IsEnabled(features::kVrShell)) {
    WebRuntimeFeatures::enableGamepadExtensions(true);
    WebRuntimeFeatures::enableWebVR(true);
  }

  // Enable explicitly enabled features, and then disable explicitly disabled
  // ones.
  if (command_line.HasSwitch(switches::kEnableBlinkFeatures)) {
    std::vector<std::string> enabled_features = base::SplitString(
        command_line.GetSwitchValueASCII(switches::kEnableBlinkFeatures),
        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const std::string& feature : enabled_features)
      WebRuntimeFeatures::enableFeatureFromString(feature, true);
  }
  if (command_line.HasSwitch(switches::kDisableBlinkFeatures)) {
    std::vector<std::string> disabled_features = base::SplitString(
        command_line.GetSwitchValueASCII(switches::kDisableBlinkFeatures),
        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const std::string& feature : disabled_features)
      WebRuntimeFeatures::enableFeatureFromString(feature, false);
  }
}

}  // namespace content
