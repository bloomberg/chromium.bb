// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include "base/command_line.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "ui/native_theme/native_theme_switches.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "media/base/android/media_codec_bridge.h"
#endif

using blink::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID)
  // MSE/EME implementation needs Android MediaCodec API.
  if (!media::MediaCodecBridge::IsAvailable()) {
    WebRuntimeFeatures::enableWebKitMediaSource(false);
    WebRuntimeFeatures::enableMediaSource(false);
    WebRuntimeFeatures::enablePrefixedEncryptedMedia(false);
  }
  // WebAudio is enabled by default on ARM and X86 and only when the
  // MediaCodec API is available.
  WebRuntimeFeatures::enableWebAudio(
      media::MediaCodecBridge::IsAvailable() &&
      ((android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM) ||
       (android_getCpuFamily() == ANDROID_CPU_FAMILY_X86)));
  // Android does not support the Gamepad API.
  WebRuntimeFeatures::enableGamepad(false);
  // Android does not have support for PagePopup
  WebRuntimeFeatures::enablePagePopup(false);
  // Android does not yet support the Web Notification API. crbug.com/115320
  WebRuntimeFeatures::enableNotifications(false);
  // Android does not yet support SharedWorker. crbug.com/154571
  WebRuntimeFeatures::enableSharedWorker(false);
  // Android does not yet support NavigatorContentUtils.
  WebRuntimeFeatures::enableNavigatorContentUtils(false);
  WebRuntimeFeatures::enableTouchIconLoading(true);
  WebRuntimeFeatures::enableOrientationEvent(true);
#else
  WebRuntimeFeatures::enableNavigatorContentUtils(true);
#endif  // defined(OS_ANDROID)
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
    WebRuntimeFeatures::enableExperimentalFeatures(true);

  SetRuntimeFeatureDefaultsForPlatform();

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::enableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableApplicationCache))
    WebRuntimeFeatures::enableApplicationCache(false);

  if (command_line.HasSwitch(switches::kDisableDesktopNotifications))
    WebRuntimeFeatures::enableNotifications(false);

  if (command_line.HasSwitch(switches::kDisableNavigatorContentUtils))
    WebRuntimeFeatures::enableNavigatorContentUtils(false);

  if (command_line.HasSwitch(switches::kDisableLocalStorage))
    WebRuntimeFeatures::enableLocalStorage(false);

  if (command_line.HasSwitch(switches::kDisableSessionStorage))
    WebRuntimeFeatures::enableSessionStorage(false);

  if (command_line.HasSwitch(switches::kDisableWebKitMediaSource))
    WebRuntimeFeatures::enableWebKitMediaSource(false);

  if (command_line.HasSwitch(switches::kDisableUnprefixedMediaSource))
    WebRuntimeFeatures::enableMediaSource(false);

  if (command_line.HasSwitch(switches::kDisableSharedWorkers))
    WebRuntimeFeatures::enableSharedWorker(false);

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableWebRTC)) {
    WebRuntimeFeatures::enableMediaStream(false);
    WebRuntimeFeatures::enablePeerConnection(false);
  }

  if (!command_line.HasSwitch(switches::kEnableSpeechRecognition))
    WebRuntimeFeatures::enableScriptedSpeech(false);
#endif

  if (command_line.HasSwitch(switches::kEnableServiceWorker))
    WebRuntimeFeatures::enableServiceWorker(true);

#if defined(OS_ANDROID)
  // WebAudio is enabled by default on ARM and X86, if the MediaCodec
  // API is available.
  WebRuntimeFeatures::enableWebAudio(
      !command_line.HasSwitch(switches::kDisableWebAudio) &&
      media::MediaCodecBridge::IsAvailable());
#else
  if (command_line.HasSwitch(switches::kDisableWebAudio))
    WebRuntimeFeatures::enableWebAudio(false);
#endif

  if (command_line.HasSwitch(switches::kEnableEncryptedMedia))
    WebRuntimeFeatures::enableEncryptedMedia(true);

  if (command_line.HasSwitch(switches::kDisablePrefixedEncryptedMedia))
    WebRuntimeFeatures::enablePrefixedEncryptedMedia(false);

  if (command_line.HasSwitch(switches::kEnableWebAnimationsSVG))
    WebRuntimeFeatures::enableWebAnimationsSVG(true);

  if (command_line.HasSwitch(switches::kEnableWebMIDI))
    WebRuntimeFeatures::enableWebMIDI(true);

  if (command_line.HasSwitch(switches::kDisableSpeechInput))
    WebRuntimeFeatures::enableSpeechInput(false);

  if (command_line.HasSwitch(switches::kDisableFileSystem))
    WebRuntimeFeatures::enableFileSystem(false);

  if (command_line.HasSwitch(switches::kEnableExperimentalCanvasFeatures))
    WebRuntimeFeatures::enableExperimentalCanvasFeatures(true);

  if (command_line.HasSwitch(switches::kEnableSpeechSynthesis))
    WebRuntimeFeatures::enableSpeechSynthesis(true);

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::enableWebGLDraftExtensions(true);

  if (command_line.HasSwitch(switches::kEnableOverlayFullscreenVideo))
    WebRuntimeFeatures::enableOverlayFullscreenVideo(true);

  if (ui::IsOverlayScrollbarEnabled())
    WebRuntimeFeatures::enableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnableFastTextAutosizing)
      && !command_line.HasSwitch(switches::kDisableFastTextAutosizing))
    WebRuntimeFeatures::enableFastTextAutosizing(true);

  if (command_line.HasSwitch(switches::kDisableRepaintAfterLayout))
    WebRuntimeFeatures::enableRepaintAfterLayout(false);

  if (command_line.HasSwitch(switches::kEnableRepaintAfterLayout))
    WebRuntimeFeatures::enableRepaintAfterLayout(true);

  if (command_line.HasSwitch(switches::kEnableTargetedStyleRecalc))
    WebRuntimeFeatures::enableTargetedStyleRecalc(true);

  if (command_line.HasSwitch(switches::kEnableBleedingEdgeRenderingFastPaths))
    WebRuntimeFeatures::enableBleedingEdgeFastPaths(true);
}

}  // namespace content
