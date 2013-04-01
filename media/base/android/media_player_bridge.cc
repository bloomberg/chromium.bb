// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/message_loop_proxy.h"
#include "jni/MediaPlayerBridge_jni.h"
#include "jni/MediaPlayer_jni.h"
#include "media/base/android/media_player_bridge_manager.h"
#include "media/base/android/media_resource_getter.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

// Time update happens every 250ms.
static const int kTimeUpdateInterval = 250;

// Because we create the media player lazily on android, the duration of the
// media is initially unknown to us. This makes the user unable to perform
// seek. To solve this problem, we use a temporary duration of 100 seconds when
// the duration is unknown. And we scale the seek position later when duration
// is available.
// TODO(qinmin): create a thread and use android MediaMetadataRetriever
// class to extract the duration.
static const int kTemporaryDuration = 100;

namespace media {

MediaPlayerBridge::MediaPlayerBridge(
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    MediaResourceGetter* resource_getter,
    bool hide_url_log,
    MediaPlayerBridgeManager* manager,
    const MediaErrorCB& media_error_cb,
    const VideoSizeChangedCB& video_size_changed_cb,
    const BufferingUpdateCB& buffering_update_cb,
    const MediaPreparedCB& media_prepared_cb,
    const PlaybackCompleteCB& playback_complete_cb,
    const SeekCompleteCB& seek_complete_cb,
    const TimeUpdateCB& time_update_cb,
    const MediaInterruptedCB& media_interrupted_cb)
    : media_error_cb_(media_error_cb),
      video_size_changed_cb_(video_size_changed_cb),
      buffering_update_cb_(buffering_update_cb),
      media_prepared_cb_(media_prepared_cb),
      playback_complete_cb_(playback_complete_cb),
      seek_complete_cb_(seek_complete_cb),
      media_interrupted_cb_(media_interrupted_cb),
      time_update_cb_(time_update_cb),
      player_id_(player_id),
      prepared_(false),
      pending_play_(false),
      url_(url),
      first_party_for_cookies_(first_party_for_cookies),
      hide_url_log_(hide_url_log),
      duration_(base::TimeDelta::FromSeconds(kTemporaryDuration)),
      width_(0),
      height_(0),
      can_pause_(true),
      can_seek_forward_(true),
      can_seek_backward_(true),
      manager_(manager),
      resource_getter_(resource_getter),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)),
      listener_(base::MessageLoopProxy::current(),
                weak_this_.GetWeakPtr()) {
  has_cookies_ = url_.SchemeIsFileSystem() || url_.SchemeIsFile();
}

MediaPlayerBridge::~MediaPlayerBridge() {
  Release();
}

void MediaPlayerBridge::InitializePlayer() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  j_media_player_.Reset(JNI_MediaPlayer::Java_MediaPlayer_Constructor(env));

  jobject j_context = base::android::GetApplicationContext();
  DCHECK(j_context);

  listener_.CreateMediaPlayerListener(j_context, j_media_player_.obj());
}

void MediaPlayerBridge::SetVideoSurface(jobject surface) {
  if (j_media_player_.is_null()) {
    if (surface == NULL)
      return;
    Prepare();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  JNI_MediaPlayer::Java_MediaPlayer_setSurface(
      env, j_media_player_.obj(), surface);
}

void MediaPlayerBridge::Prepare() {
  if (j_media_player_.is_null())
    InitializePlayer();

  if (has_cookies_) {
    GetCookiesCallback(cookies_);
  } else {
    resource_getter_->GetCookies(url_, first_party_for_cookies_, base::Bind(
        &MediaPlayerBridge::GetCookiesCallback, weak_this_.GetWeakPtr()));
  }
}

void MediaPlayerBridge::GetCookiesCallback(const std::string& cookies) {
  cookies_ = cookies;
  has_cookies_ = true;

  if (j_media_player_.is_null())
    return;

  if (url_.SchemeIsFileSystem()) {
    resource_getter_->GetPlatformPathFromFileSystemURL(url_, base::Bind(
        &MediaPlayerBridge::SetDataSource, weak_this_.GetWeakPtr()));
  } else {
    SetDataSource(url_.spec());
  }
}

void MediaPlayerBridge::SetDataSource(const std::string& url) {
  if (j_media_player_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  // Create a Java String for the URL.
  ScopedJavaLocalRef<jstring> j_url_string = ConvertUTF8ToJavaString(env, url);
  ScopedJavaLocalRef<jstring> j_cookies = ConvertUTF8ToJavaString(
      env, cookies_);

  jobject j_context = base::android::GetApplicationContext();
  DCHECK(j_context);

  if (Java_MediaPlayerBridge_setDataSource(
      env, j_media_player_.obj(), j_context, j_url_string.obj(),
      j_cookies.obj(), hide_url_log_)) {
    if (manager_)
      manager_->RequestMediaResources(this);
    JNI_MediaPlayer::Java_MediaPlayer_prepareAsync(
        env, j_media_player_.obj());
  } else {
    media_error_cb_.Run(player_id_, MEDIA_ERROR_FORMAT);
  }
}

void MediaPlayerBridge::Start() {
  if (j_media_player_.is_null()) {
    pending_play_ = true;
    Prepare();
  } else {
    if (prepared_)
      StartInternal();
    else
      pending_play_ = true;
  }
}

void MediaPlayerBridge::Pause() {
  if (j_media_player_.is_null()) {
    pending_play_ = false;
  } else {
    if (prepared_ && IsPlaying())
      PauseInternal();
    else
      pending_play_ = false;
  }
}

bool MediaPlayerBridge::IsPlaying() {
  if (!prepared_)
    return pending_play_;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  jboolean result = JNI_MediaPlayer::Java_MediaPlayer_isPlaying(
      env, j_media_player_.obj());
  return result;
}

int MediaPlayerBridge::GetVideoWidth() {
  if (!prepared_)
    return width_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return JNI_MediaPlayer::Java_MediaPlayer_getVideoWidth(
      env, j_media_player_.obj());
}

int MediaPlayerBridge::GetVideoHeight() {
  if (!prepared_)
    return height_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return JNI_MediaPlayer::Java_MediaPlayer_getVideoHeight(
      env, j_media_player_.obj());
}

void MediaPlayerBridge::SeekTo(base::TimeDelta time) {
  // Record the time to seek when OnMediaPrepared() is called.
  pending_seek_ = time;

  if (j_media_player_.is_null())
    Prepare();
  else if (prepared_)
    SeekInternal(time);
}

base::TimeDelta MediaPlayerBridge::GetCurrentTime() {
  if (!prepared_)
    return pending_seek_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::TimeDelta::FromMilliseconds(
      JNI_MediaPlayer::Java_MediaPlayer_getCurrentPosition(
          env, j_media_player_.obj()));
}

base::TimeDelta MediaPlayerBridge::GetDuration() {
  if (!prepared_)
    return duration_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::TimeDelta::FromMilliseconds(
      JNI_MediaPlayer::Java_MediaPlayer_getDuration(
          env, j_media_player_.obj()));
}

void MediaPlayerBridge::Release() {
  if (j_media_player_.is_null())
    return;

  time_update_timer_.Stop();
  if (prepared_)
    pending_seek_ = GetCurrentTime();
  if (manager_)
    manager_->ReleaseMediaResources(this);
  prepared_ = false;
  pending_play_ = false;
  SetVideoSurface(NULL);

  JNIEnv* env = base::android::AttachCurrentThread();
  JNI_MediaPlayer::Java_MediaPlayer_release(env, j_media_player_.obj());
  j_media_player_.Reset();

  listener_.ReleaseMediaPlayerListenerResources();
}

void MediaPlayerBridge::SetVolume(float left_volume, float right_volume) {
  if (j_media_player_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  JNI_MediaPlayer::Java_MediaPlayer_setVolume(
      env, j_media_player_.obj(), left_volume, right_volume);
}

void MediaPlayerBridge::DoTimeUpdate() {
  base::TimeDelta current = GetCurrentTime();
  time_update_cb_.Run(player_id_, current);
}

void MediaPlayerBridge::OnMediaError(int error_type) {
  media_error_cb_.Run(player_id_, error_type);
}

void MediaPlayerBridge::OnVideoSizeChanged(int width, int height) {
  width_ = width;
  height_ = height;
  video_size_changed_cb_.Run(player_id_, width, height);
}

void MediaPlayerBridge::OnBufferingUpdate(int percent) {
  buffering_update_cb_.Run(player_id_, percent);
}

void MediaPlayerBridge::OnPlaybackComplete() {
  time_update_timer_.Stop();
  playback_complete_cb_.Run(player_id_);
}


void MediaPlayerBridge::OnMediaInterrupted() {
  time_update_timer_.Stop();
  media_interrupted_cb_.Run(player_id_);
}

void MediaPlayerBridge::OnSeekComplete() {
  seek_complete_cb_.Run(player_id_, GetCurrentTime());
}

void MediaPlayerBridge::OnMediaPrepared() {
  if (j_media_player_.is_null())
    return;

  prepared_ = true;

  base::TimeDelta dur = duration_;
  duration_ = GetDuration();

  if (duration_ != dur && 0 != dur.InMilliseconds()) {
    // Scale the |pending_seek_| according to the new duration.
    pending_seek_ = base::TimeDelta::FromSeconds(
        pending_seek_.InSecondsF() * duration_.InSecondsF() / dur.InSecondsF());
  }

  // If media player was recovered from a saved state, consume all the pending
  // events.
  SeekInternal(pending_seek_);

  if (pending_play_) {
    StartInternal();
    pending_play_ = false;
  }

  GetMetadata();

  media_prepared_cb_.Run(player_id_, duration_);
}

void MediaPlayerBridge::GetMetadata() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jobject> allowedOperations =
      Java_MediaPlayerBridge_getAllowedOperations(env, j_media_player_.obj());
  can_pause_ = Java_AllowedOperations_canPause(env, allowedOperations.obj());
  can_seek_forward_ = Java_AllowedOperations_canSeekForward(
      env, allowedOperations.obj());
  can_seek_backward_ = Java_AllowedOperations_canSeekBackward(
      env, allowedOperations.obj());
}

void MediaPlayerBridge::StartInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  JNI_MediaPlayer::Java_MediaPlayer_start(env, j_media_player_.obj());
  if (!time_update_timer_.IsRunning()) {
    time_update_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kTimeUpdateInterval),
        this, &MediaPlayerBridge::DoTimeUpdate);
  }
}

void MediaPlayerBridge::PauseInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  JNI_MediaPlayer::Java_MediaPlayer_pause(env, j_media_player_.obj());
  time_update_timer_.Stop();
}

void MediaPlayerBridge::SeekInternal(base::TimeDelta time) {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  int time_msec = static_cast<int>(time.InMilliseconds());
  JNI_MediaPlayer::Java_MediaPlayer_seekTo(
      env, j_media_player_.obj(), time_msec);
}

bool MediaPlayerBridge::RegisterMediaPlayerBridge(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  DCHECK(g_MediaPlayerBridge_clazz);
  if (ret)
    ret = JNI_MediaPlayer::RegisterNativesImpl(env);
  return ret;
}

}  // namespace media
