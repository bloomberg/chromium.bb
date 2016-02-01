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

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "base/android/build_info.h"
#include "media/base/android/media_codec_util.h"
#endif

using blink::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID)
  // MSE/EME implementation needs Android MediaCodec API.
  if (!media::MediaCodecUtil::IsMediaCodecAvailable()) {
    WebRuntimeFeatures::enableMediaSource(false);
    WebRuntimeFeatures::enablePrefixedEncryptedMedia(false);
    WebRuntimeFeatures::enableEncryptedMedia(false);
  }

  // Android does not have support for PagePopup
  WebRuntimeFeatures::enablePagePopup(false);
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
  WebRuntimeFeatures::enableNewMediaPlaybackUi(true);
  // Android does not yet support switching of audio output devices
  WebRuntimeFeatures::enableAudioOutputDevices(false);
#else
  WebRuntimeFeatures::enableNavigatorContentUtils(true);
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(USE_AURA)
  WebRuntimeFeatures::enableCompositedSelectionUpdate(true);
#endif

#if !(defined OS_ANDROID || defined OS_CHROMEOS || defined OS_IOS)
    // Only Android, ChromeOS, and IOS support NetInfo right now.
    WebRuntimeFeatures::enableNetworkInformation(false);
#endif
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
    WebRuntimeFeatures::enableExperimentalFeatures(true);

  if (base::FeatureList::IsEnabled(features::kExperimentalFramework))
    WebRuntimeFeatures::enableExperimentalFramework(true);

  if (command_line.HasSwitch(switches::kEnableWebBluetooth))
    WebRuntimeFeatures::enableWebBluetooth(true);

  SetRuntimeFeatureDefaultsForPlatform();

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::enableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableMediaSource))
    WebRuntimeFeatures::enableMediaSource(false);

  if (command_line.HasSwitch(switches::kDisableNotifications)) {
    WebRuntimeFeatures::enableNotifications(false);

    // Chrome's Push Messaging implementation relies on Web Notifications.
    WebRuntimeFeatures::enablePushMessaging(false);
  }

  if (command_line.HasSwitch(switches::kDisableSharedWorkers))
    WebRuntimeFeatures::enableSharedWorker(false);

  if (command_line.HasSwitch(switches::kDisableWebAudio))
    WebRuntimeFeatures::enableWebAudio(false);

  if (command_line.HasSwitch(switches::kDisableSpeechAPI))
    WebRuntimeFeatures::enableScriptedSpeech(false);

  if (command_line.HasSwitch(switches::kDisableEncryptedMedia))
    WebRuntimeFeatures::enableEncryptedMedia(false);

  if (command_line.HasSwitch(switches::kEnablePrefixedEncryptedMedia))
    WebRuntimeFeatures::enablePrefixedEncryptedMedia(true);

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

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::enableWebGLDraftExtensions(true);

  if (command_line.HasSwitch(switches::kEnableWebGLImageChromium))
    WebRuntimeFeatures::enableWebGLImageChromium(true);

  if (command_line.HasSwitch(switches::kForceOverlayFullscreenVideo))
    WebRuntimeFeatures::forceOverlayFullscreenVideo(true);

  if (ui::IsOverlayScrollbarEnabled())
    WebRuntimeFeatures::enableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnablePreciseMemoryInfo))
    WebRuntimeFeatures::enablePreciseMemoryInfo(true);

  if (command_line.HasSwitch(switches::kEnableNetworkInformation) ||
      command_line.HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    WebRuntimeFeatures::enableNetworkInformation(true);
  }

  if (command_line.HasSwitch(switches::kEnableCredentialManagerAPI))
    WebRuntimeFeatures::enableCredentialManagerAPI(true);

  if (command_line.HasSwitch(switches::kReducedReferrerGranularity))
    WebRuntimeFeatures::enableReducedReferrerGranularity(true);

  if (command_line.HasSwitch(switches::kDisablePermissionsAPI))
    WebRuntimeFeatures::enablePermissionsAPI(false);

  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::enableV8IdleTasks(false);
  else
    WebRuntimeFeatures::enableV8IdleTasks(true);

  if (command_line.HasSwitch(switches::kEnableUnsafeES3APIs))
    WebRuntimeFeatures::enableUnsafeES3APIs(true);

  if (command_line.HasSwitch(switches::kEnableWebVR)) {
    WebRuntimeFeatures::enableWebVR(true);
    WebRuntimeFeatures::enableFeatureFromString(
        std::string("GeometryInterfaces"), true);
  }

  if (command_line.HasSwitch(switches::kDisablePresentationAPI))
    WebRuntimeFeatures::enablePresentationAPI(false);

  if (base::FeatureList::IsEnabled(features::kWebFontsIntervention))
    WebRuntimeFeatures::enableWebFontsIntervention(true);

  if (command_line.HasSwitch(switches::kEnableSlimmingPaintV2))
    WebRuntimeFeatures::enableSlimmingPaintV2(true);

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
