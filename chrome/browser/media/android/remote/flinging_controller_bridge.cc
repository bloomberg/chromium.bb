// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/remote/flinging_controller_bridge.h"

#include "jni/FlingingControllerBridge_jni.h"

namespace media_router {

FlingingControllerBridge::FlingingControllerBridge(
    base::android::ScopedJavaGlobalRef<jobject> controller)
    : j_flinging_controller_bridge_(controller) {}

FlingingControllerBridge::~FlingingControllerBridge() = default;

void FlingingControllerBridge::Play() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_play(env, j_flinging_controller_bridge_);
}

void FlingingControllerBridge::Pause() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_pause(env, j_flinging_controller_bridge_);
}

void FlingingControllerBridge::SetMute(bool mute) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_setMute(env, j_flinging_controller_bridge_,
                                        mute);
}

void FlingingControllerBridge::SetVolume(float volume) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_setVolume(env, j_flinging_controller_bridge_,
                                          volume);
}

void FlingingControllerBridge::Seek(base::TimeDelta time) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_seek(env, j_flinging_controller_bridge_,
                                     time.InMilliseconds());
}

media::MediaController* FlingingControllerBridge::GetMediaController() {
  return this;
}

void FlingingControllerBridge::AddMediaStatusObserver(
    media::MediaStatusObserver* observer) {
  NOTREACHED();
}
void FlingingControllerBridge::RemoveMediaStatusObserver(
    media::MediaStatusObserver* observer) {
  NOTREACHED();
}

base::TimeDelta FlingingControllerBridge::GetApproximateCurrentTime() {
  // TODO(https://crbug.com/830871): Implement this method.
  return base::TimeDelta();
}

}  // namespace media_router
