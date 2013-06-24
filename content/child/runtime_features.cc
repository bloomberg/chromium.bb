// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#endif

using WebKit::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID) && !defined(GOOGLE_TV)
  WebRuntimeFeatures::enableWebKitMediaSource(false);
  WebRuntimeFeatures::enableLegacyEncryptedMedia(false);
  WebRuntimeFeatures::enableEncryptedMedia(false);
#endif

#if defined(OS_ANDROID)
  bool enable_webaudio = false;
#if defined(ARCH_CPU_ARMEL)
  enable_webaudio =
      ((android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0);
#endif
  WebRuntimeFeatures::enableWebAudio(enable_webaudio);
  // Android does not support the Gamepad API.
  WebRuntimeFeatures::enableGamepad(false);
  // input[type=week] in Android is incomplete. crbug.com/135938
  WebRuntimeFeatures::enableInputTypeWeek(false);
  // Android does not have support for PagePopup
  WebRuntimeFeatures::enablePagePopup(false);
  // datalist on Android is not enabled
  WebRuntimeFeatures::enableDataListElement(false);
#endif
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const CommandLine& command_line) {
  WebRuntimeFeatures::enableStableFeatures(true);

  if (command_line.HasSwitch(switches::kEnableExperimentalWebKitFeatures))
    WebRuntimeFeatures::enableExperimentalFeatures(true);

  SetRuntimeFeatureDefaultsForPlatform();

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::enableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableApplicationCache))
    WebRuntimeFeatures::enableApplicationCache(false);

  if (command_line.HasSwitch(switches::kDisableDesktopNotifications))
    WebRuntimeFeatures::enableNotifications(false);

  if (command_line.HasSwitch(switches::kDisableLocalStorage))
    WebRuntimeFeatures::enableLocalStorage(false);

  if (command_line.HasSwitch(switches::kDisableSessionStorage))
    WebRuntimeFeatures::enableSessionStorage(false);

  if (command_line.HasSwitch(switches::kDisableGeolocation))
    WebRuntimeFeatures::enableGeolocation(false);

#if defined(OS_ANDROID) && !defined(GOOGLE_TV)
  if (command_line.HasSwitch(switches::kEnableWebKitMediaSource))
    WebRuntimeFeatures::enableWebKitMediaSource(true);
#else
  if (command_line.HasSwitch(switches::kDisableWebKitMediaSource))
    WebRuntimeFeatures::enableWebKitMediaSource(false);
#endif

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableWebRTC)) {
    WebRuntimeFeatures::enableMediaStream(false);
    WebRuntimeFeatures::enablePeerConnection(false);
  }

  if (!command_line.HasSwitch(switches::kEnableSpeechRecognition))
    WebRuntimeFeatures::enableScriptedSpeech(false);
#endif

  if (command_line.HasSwitch(switches::kDisableWebAudio))
    WebRuntimeFeatures::enableWebAudio(false);

  if (command_line.HasSwitch(switches::kDisableFullScreen))
    WebRuntimeFeatures::enableFullscreen(false);

  if (command_line.HasSwitch(switches::kEnableEncryptedMedia))
    WebRuntimeFeatures::enableEncryptedMedia(true);

  if (command_line.HasSwitch(switches::kDisableLegacyEncryptedMedia))
    WebRuntimeFeatures::enableLegacyEncryptedMedia(false);

  if (command_line.HasSwitch(switches::kEnableWebMIDI))
    WebRuntimeFeatures::enableWebMIDI(true);

  if (command_line.HasSwitch(switches::kEnableDeviceMotion))
      WebRuntimeFeatures::enableDeviceMotion(true);

  if (command_line.HasSwitch(switches::kDisableDeviceOrientation))
    WebRuntimeFeatures::enableDeviceOrientation(false);

  if (command_line.HasSwitch(switches::kDisableSpeechInput))
    WebRuntimeFeatures::enableSpeechInput(false);

  if (command_line.HasSwitch(switches::kDisableFileSystem))
    WebRuntimeFeatures::enableFileSystem(false);

  if (command_line.HasSwitch(switches::kDisableJavaScriptI18NAPI))
    WebRuntimeFeatures::enableJavaScriptI18NAPI(false);

  if (command_line.HasSwitch(switches::kEnableExperimentalCanvasFeatures))
    WebRuntimeFeatures::enableExperimentalCanvasFeatures(true);

  if (command_line.HasSwitch(switches::kEnableSpeechSynthesis))
    WebRuntimeFeatures::enableSpeechSynthesis(true);

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::enableWebGLDraftExtensions(true);
}

}  // namespace content
