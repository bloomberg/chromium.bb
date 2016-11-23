// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"

#include "media/audio/android/audio_manager_android.h"
#include "media/audio/android/audio_record_input.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/android/media_player_listener.h"
#include "media/base/android/media_server_crash_listener.h"

namespace media {

static base::android::RegistrationMethod kMediaRegisteredMethods[] = {
    {"AudioManagerAndroid", AudioManagerAndroid::RegisterAudioManager},
    {"AudioRecordInput", AudioRecordInputStream::RegisterAudioRecordInput},
    {"MediaDrmBridge", MediaDrmBridge::RegisterMediaDrmBridge},
    {"MediaPlayerBridge", MediaPlayerBridge::RegisterMediaPlayerBridge},
    {"MediaPlayerListener", MediaPlayerListener::RegisterMediaPlayerListener},
    {"MediaServerCrashListener",
     MediaServerCrashListener::RegisterMediaServerCrashListener},
};

bool RegisterJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kMediaRegisteredMethods, arraysize(kMediaRegisteredMethods));
}

}  // namespace media
