// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_listener.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/android/media_player_bridge.h"

// Auto generated jni class from MediaPlayerListener.java.
// Check base/android/jni_generator/golden_sample_for_tests_jni.h for example.
#include "jni/MediaPlayerListener_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaLocalRef;

namespace media {

MediaPlayerListener::MediaPlayerListener(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    base::WeakPtr<MediaPlayerBridge> media_player)
    : message_loop_(message_loop),
      media_player_(media_player) {
  DCHECK(message_loop_);
  DCHECK(media_player_);
}

MediaPlayerListener::~MediaPlayerListener() {}

ScopedJavaLocalRef<jobject> MediaPlayerListener::CreateMediaPlayerListener() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jobject> j_listener(
      Java_MediaPlayerListener_create(env, reinterpret_cast<intptr_t>(this)));
  DCHECK(!j_listener.is_null());
  return j_listener;
}

void MediaPlayerListener::OnMediaError(
    JNIEnv* /* env */, jobject /* obj */, jint error_type) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnMediaError, media_player_, error_type));
}

void MediaPlayerListener::OnVideoSizeChanged(
    JNIEnv* /* env */, jobject /* obj */, jint width, jint height) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnVideoSizeChanged, media_player_,
      width, height));
}

void MediaPlayerListener::OnBufferingUpdate(
    JNIEnv* /* env */, jobject /* obj */, jint percent) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnBufferingUpdate, media_player_, percent));
}

void MediaPlayerListener::OnPlaybackComplete(
    JNIEnv* /* env */, jobject /* obj */) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnPlaybackComplete, media_player_));
}

void MediaPlayerListener::OnSeekComplete(
    JNIEnv* /* env */, jobject /* obj */) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnSeekComplete, media_player_));
}

void MediaPlayerListener::OnMediaPrepared(
    JNIEnv* /* env */, jobject /* obj */) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnMediaPrepared, media_player_));
}

bool MediaPlayerListener::RegisterMediaPlayerListener(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  DCHECK(g_MediaPlayerListener_clazz);
  return ret;
}

}  // namespace media
