// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_listener.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "jni/MediaPlayerListener_jni.h"
#include "media/base/android/media_player_android.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaLocalRef;

namespace media {

MediaPlayerListenerProxy::MediaPlayerListenerProxy(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<MediaPlayerAndroid> media_player)
    : task_runner_(task_runner),
      media_player_(media_player) {
  DCHECK(task_runner_.get());
  DCHECK(media_player_);
}

void MediaPlayerListenerProxy::OnMediaError(int error_type) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlayerListenerProxy::OnMediaError, this, error_type));
    return;
  }

  if (media_player_)
    media_player_->OnMediaError(error_type);
}

void MediaPlayerListenerProxy::OnVideoSizeChanged(int width, int height) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlayerListenerProxy::OnVideoSizeChanged,
                   this, width, height));
    return;
  }

  if (media_player_)
    media_player_->OnVideoSizeChanged(width, height);
}

void MediaPlayerListenerProxy::OnBufferingUpdate(int percent) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlayerListenerProxy::OnBufferingUpdate,
                   this, percent));
    return;
  }

  if (media_player_)
    media_player_->OnBufferingUpdate(percent);
}

void MediaPlayerListenerProxy::OnPlaybackComplete() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlayerListenerProxy::OnPlaybackComplete, this));
    return;
  }

  if (media_player_)
    media_player_->OnPlaybackComplete();
}

void MediaPlayerListenerProxy::OnSeekComplete() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlayerListenerProxy::OnSeekComplete, this));
    return;
  }

  if (media_player_)
    media_player_->OnSeekComplete();
}

void MediaPlayerListenerProxy::OnMediaPrepared() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlayerListenerProxy::OnMediaPrepared, this));
    return;
  }

  if (media_player_)
    media_player_->OnMediaPrepared();
}

void MediaPlayerListenerProxy::OnMediaInterrupted() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlayerListenerProxy::OnMediaInterrupted, this));
    return;
  }

  if (media_player_)
    media_player_->OnMediaInterrupted();
}

MediaPlayerListenerProxy::~MediaPlayerListenerProxy() {}

MediaPlayerListener::MediaPlayerListener(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<MediaPlayerAndroid> media_player)
    : proxy_(new MediaPlayerListenerProxy(task_runner, media_player)) {
}

MediaPlayerListener::~MediaPlayerListener() {}

void MediaPlayerListener::CreateMediaPlayerListener(
    jobject context, jobject media_player) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (j_media_player_listener_.is_null()) {
    j_media_player_listener_.Reset(Java_MediaPlayerListener_create(
        env, reinterpret_cast<intptr_t>(this), context, media_player));
  }
}

void MediaPlayerListener::ReleaseMediaPlayerListenerResources() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (!j_media_player_listener_.is_null()) {
    Java_MediaPlayerListener_releaseResources(
        env, j_media_player_listener_.obj());
  }
  j_media_player_listener_.Reset();
}

void MediaPlayerListener::OnMediaError(
    JNIEnv* /* env */, jobject /* obj */, jint error_type) {
  proxy_->OnMediaError(error_type);
}

void MediaPlayerListener::OnVideoSizeChanged(
    JNIEnv* /* env */, jobject /* obj */, jint width, jint height) {
  proxy_->OnVideoSizeChanged(width, height);
}

void MediaPlayerListener::OnBufferingUpdate(
    JNIEnv* /* env */, jobject /* obj */, jint percent) {
  proxy_->OnBufferingUpdate(percent);
}

void MediaPlayerListener::OnPlaybackComplete(
    JNIEnv* /* env */, jobject /* obj */) {
  proxy_->OnPlaybackComplete();
}

void MediaPlayerListener::OnSeekComplete(
    JNIEnv* /* env */, jobject /* obj */) {
  proxy_->OnSeekComplete();
}

void MediaPlayerListener::OnMediaPrepared(
    JNIEnv* /* env */, jobject /* obj */) {
  proxy_->OnMediaPrepared();
}

void MediaPlayerListener::OnMediaInterrupted(
    JNIEnv* /* env */, jobject /* obj */) {
  proxy_->OnMediaInterrupted();
}

bool MediaPlayerListener::RegisterMediaPlayerListener(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace media
