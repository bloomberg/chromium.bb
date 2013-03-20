// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_jni_registrar.h"

#include "base/basictypes.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"

#include "media/audio/audio_manager_base.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/android/media_player_listener.h"
#include "media/video/capture/android/video_capture_device_android.h"

namespace media {

static base::android::RegistrationMethod kMediaRegisteredMethods[] = {
  { "AudioManagerBase",
    AudioManagerBase::RegisterAudioManager },
  { "MediaPlayerBridge",
    MediaPlayerBridge::RegisterMediaPlayerBridge },
  { "MediaPlayerListener",
    MediaPlayerListener::RegisterMediaPlayerListener },
  { "VideoCaptureDevice",
    VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice },
};

bool RegisterJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kMediaRegisteredMethods, arraysize(kMediaRegisteredMethods));
}

}  // namespace media
