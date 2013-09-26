// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "base/android/build_info.h"
#endif

using WebKit::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID)
#if !defined(GOOGLE_TV)
  // MSE/EME implementation needs Android MediaCodec API that was introduced
  // in JellyBrean.
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 16) {
    WebRuntimeFeatures::enableWebKitMediaSource(false);
    WebRuntimeFeatures::enableLegacyEncryptedMedia(false);
  }
#endif  // !defined(GOOGLE_TV)
  bool enable_webaudio = false;
#if defined(ARCH_CPU_ARMEL)
  // WebAudio needs Android MediaCodec API that was introduced in
  // JellyBean, and also currently needs NEON support for the FFT.
  enable_webaudio =
      (base::android::BuildInfo::GetInstance()->sdk_int() >= 16) &&
      ((android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0);
#endif  // defined(ARCH_CPU_ARMEL)
  WebRuntimeFeatures::enableWebAudio(enable_webaudio);
  // Android does not support the Gamepad API.
  WebRuntimeFeatures::enableGamepad(false);
  // Android does not have support for PagePopup
  WebRuntimeFeatures::enablePagePopup(false);
  // datalist on Android is not enabled
  WebRuntimeFeatures::enableDataListElement(false);
  // Android does not yet support the Web Notification API. crbug.com/115320
  WebRuntimeFeatures::enableNotifications(false);
#endif  // defined(OS_ANDROID)
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const CommandLine& command_line) {
  WebRuntimeFeatures::enableStableFeatures(true);

  if (command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
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

  if (command_line.HasSwitch(switches::kDisableWebKitMediaSource))
    WebRuntimeFeatures::enableWebKitMediaSource(false);

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

  if (command_line.HasSwitch(switches::kEnableWebAnimationsCSS))
    WebRuntimeFeatures::enableWebAnimationsCSS();

  if (command_line.HasSwitch(switches::kEnableWebAnimationsSVG))
    WebRuntimeFeatures::enableWebAnimationsSVG();

  if (command_line.HasSwitch(switches::kEnableWebMIDI))
    WebRuntimeFeatures::enableWebMIDI(true);

  if (command_line.HasSwitch(switches::kDisableDeviceMotion))
    WebRuntimeFeatures::enableDeviceMotion(false);

  if (command_line.HasSwitch(switches::kDisableDeviceOrientation))
    WebRuntimeFeatures::enableDeviceOrientation(false);

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

  if (command_line.HasSwitch(switches::kEnableHTMLImports))
    WebRuntimeFeatures::enableHTMLImports(true);

  if (command_line.HasSwitch(switches::kEnableOverlayFullscreenVideo))
    WebRuntimeFeatures::enableOverlayFullscreenVideo(true);

  if (command_line.HasSwitch(switches::kEnableOverlayScrollbars))
    WebRuntimeFeatures::enableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnableInputModeAttribute))
    WebRuntimeFeatures::enableInputModeAttribute(true);
}

}  // namespace content
