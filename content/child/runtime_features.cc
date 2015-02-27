// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_switches.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "base/android/build_info.h"
#include "base/metrics/field_trial.h"
#include "media/base/android/media_codec_bridge.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using blink::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID)
  // MSE/EME implementation needs Android MediaCodec API.
  if (!media::MediaCodecBridge::IsAvailable()) {
    WebRuntimeFeatures::enableMediaSource(false);
    WebRuntimeFeatures::enablePrefixedEncryptedMedia(false);
    WebRuntimeFeatures::enableEncryptedMedia(false);
  }
  // WebAudio is enabled by default but only when the MediaCodec API
  // is available.
  AndroidCpuFamily cpu_family = android_getCpuFamily();
  WebRuntimeFeatures::enableWebAudio(
      media::MediaCodecBridge::IsAvailable() &&
      ((cpu_family == ANDROID_CPU_FAMILY_ARM) ||
       (cpu_family == ANDROID_CPU_FAMILY_ARM64) ||
       (cpu_family == ANDROID_CPU_FAMILY_X86) ||
       (cpu_family == ANDROID_CPU_FAMILY_MIPS)));

  // Android does not have support for PagePopup
  WebRuntimeFeatures::enablePagePopup(false);
  // Android does not yet support SharedWorker. crbug.com/154571
  WebRuntimeFeatures::enableSharedWorker(false);
  // Android does not yet support NavigatorContentUtils.
  WebRuntimeFeatures::enableNavigatorContentUtils(false);
  WebRuntimeFeatures::enableTouchIconLoading(true);
  WebRuntimeFeatures::enableOrientationEvent(true);
  WebRuntimeFeatures::enableFastMobileScrolling(true);
  WebRuntimeFeatures::enableMediaCapture(true);
  WebRuntimeFeatures::enableCompositedSelectionUpdate(true);
  // If navigation transitions gets activated via field trial, enable it in
  // blink. We don't set this to false in case the user has manually enabled
  // the feature via experimental web platform features.
  if (base::FieldTrialList::FindFullName("NavigationTransitions") == "Enabled")
    WebRuntimeFeatures::enableNavigationTransitions(true);
  // Android won't be able to reliably support non-persistent notifications, the
  // intended behavior for which is in flux by itself.
  WebRuntimeFeatures::enableNotificationConstructor(false);
#else
  WebRuntimeFeatures::enableNavigatorContentUtils(true);
#endif  // defined(OS_ANDROID)

#if !(defined OS_ANDROID || defined OS_CHROMEOS || defined OS_IOS)
    // Only Android, ChromeOS, and IOS support NetInfo right now.
    WebRuntimeFeatures::enableNetworkInformation(false);
#endif

#if defined(OS_WIN)
  // Screen Orientation API is currently broken on Windows 8 Metro mode and
  // until we can find how to disable it only for Blink instances running in a
  // renderer process in Metro, we need to disable the API altogether for Win8.
  // See http://crbug.com/400846
  if (base::win::OSInfo::GetInstance()->version() >= base::win::VERSION_WIN8)
    WebRuntimeFeatures::enableScreenOrientation(false);
#endif // OS_WIN
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
    WebRuntimeFeatures::enableExperimentalFeatures(true);

  SetRuntimeFeatureDefaultsForPlatform();

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::enableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableBlinkScheduler))
    WebRuntimeFeatures::enableBlinkScheduler(false);

  if (command_line.HasSwitch(switches::kDisableLocalStorage))
    WebRuntimeFeatures::enableLocalStorage(false);

  if (command_line.HasSwitch(switches::kDisableMediaSource))
    WebRuntimeFeatures::enableMediaSource(false);

  if (command_line.HasSwitch(switches::kDisableNotifications)) {
    WebRuntimeFeatures::enableNotifications(false);

    // Chrome's Push Messaging implementation relies on Web Notifications.
    WebRuntimeFeatures::enablePushMessaging(false);
  }

  if (command_line.HasSwitch(switches::kDisableSharedWorkers))
    WebRuntimeFeatures::enableSharedWorker(false);

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableWebRTC))
    WebRuntimeFeatures::enablePeerConnection(false);

  // WebAudio is enabled by default on ARM and X86, if the MediaCodec
  // API is available.
  WebRuntimeFeatures::enableWebAudio(
      !command_line.HasSwitch(switches::kDisableWebAudio) &&
      media::MediaCodecBridge::IsAvailable());
#else
  if (command_line.HasSwitch(switches::kDisableWebAudio))
    WebRuntimeFeatures::enableWebAudio(false);
#endif

  if (command_line.HasSwitch(switches::kDisableEncryptedMedia))
    WebRuntimeFeatures::enableEncryptedMedia(false);

  if (command_line.HasSwitch(switches::kDisablePrefixedEncryptedMedia))
    WebRuntimeFeatures::enablePrefixedEncryptedMedia(false);

  if (command_line.HasSwitch(switches::kEnableWebMIDI))
    WebRuntimeFeatures::enableWebMIDI(true);

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

  if (command_line.HasSwitch(switches::kEnableOverlayFullscreenVideo))
    WebRuntimeFeatures::enableOverlayFullscreenVideo(true);

  if (ui::IsOverlayScrollbarEnabled())
    WebRuntimeFeatures::enableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnableBleedingEdgeRenderingFastPaths))
    WebRuntimeFeatures::enableBleedingEdgeFastPaths(true);

  if (command_line.HasSwitch(switches::kEnablePreciseMemoryInfo))
    WebRuntimeFeatures::enablePreciseMemoryInfo(true);

  if (command_line.HasSwitch(switches::kEnableNetworkInformation) ||
      command_line.HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    WebRuntimeFeatures::enableNetworkInformation(true);
  }

  if (command_line.HasSwitch(switches::kEnableCredentialManagerAPI))
    WebRuntimeFeatures::enableCredentialManagerAPI(true);

  if (command_line.HasSwitch(switches::kDisableSVG1DOM)) {
    WebRuntimeFeatures::enableSVG1DOM(false);
  }

  if (command_line.HasSwitch(switches::kReducedReferrerGranularity))
    WebRuntimeFeatures::enableReducedReferrerGranularity(true);

  if (command_line.HasSwitch(switches::kEnablePushMessagePayload))
    WebRuntimeFeatures::enablePushMessagingData(true);

  if (command_line.HasSwitch(switches::kEnablePushMessagingHasPermission))
    WebRuntimeFeatures::enablePushMessagingHasPermission(true);

  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::enableV8IdleTasks(false);
  else
    WebRuntimeFeatures::enableV8IdleTasks(true);

  if (command_line.HasSwitch(switches::kEnableUnsafeES3APIs))
    WebRuntimeFeatures::enableUnsafeES3APIs(true);

  // Enable explicitly enabled features, and then disable explicitly disabled
  // ones.
  if (command_line.HasSwitch(switches::kEnableBlinkFeatures)) {
    std::vector<std::string> enabled_features;
    base::SplitString(
        command_line.GetSwitchValueASCII(switches::kEnableBlinkFeatures), ',',
        &enabled_features);
    for (const std::string& feature : enabled_features) {
      WebRuntimeFeatures::enableFeatureFromString(
          blink::WebString::fromLatin1(feature), true);
    }
  }
  if (command_line.HasSwitch(switches::kDisableBlinkFeatures)) {
    std::vector<std::string> disabled_features;
    base::SplitString(
        command_line.GetSwitchValueASCII(switches::kDisableBlinkFeatures), ',',
        &disabled_features);
    for (const std::string& feature : disabled_features) {
      WebRuntimeFeatures::enableFeatureFromString(
          blink::WebString::fromLatin1(feature), false);
    }
  }
}

}  // namespace content
