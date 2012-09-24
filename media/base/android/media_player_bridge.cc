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
#include "media/base/android/cookie_getter.h"
#include "media/base/android/media_player_bridge_manager.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::GetMethodID;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// These constants are from the android source tree and need to be kept in
// sync with android/media/MediaMetadata.java.
static const jint kPauseAvailable = 1;
static const jint kSeekBackwardAvailable = 2;
static const jint kSeekForwardAvailable = 3;

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
    const std::string& url,
    const std::string& first_party_for_cookies,
    CookieGetter* cookie_getter,
    bool hide_url_log,
    MediaPlayerBridgeManager* manager,
    const MediaErrorCB& media_error_cb,
    const VideoSizeChangedCB& video_size_changed_cb,
    const BufferingUpdateCB& buffering_update_cb,
    const MediaPreparedCB& media_prepared_cb,
    const PlaybackCompleteCB& playback_complete_cb,
    const SeekCompleteCB& seek_complete_cb,
    const TimeUpdateCB& time_update_cb)
    : media_error_cb_(media_error_cb),
      video_size_changed_cb_(video_size_changed_cb),
      buffering_update_cb_(buffering_update_cb),
      media_prepared_cb_(media_prepared_cb),
      playback_complete_cb_(playback_complete_cb),
      seek_complete_cb_(seek_complete_cb),
      time_update_cb_(time_update_cb),
      player_id_(player_id),
      prepared_(false),
      pending_play_(false),
      url_(url),
      first_party_for_cookies_(first_party_for_cookies),
      has_cookies_(false),
      hide_url_log_(hide_url_log),
      duration_(base::TimeDelta::FromSeconds(kTemporaryDuration)),
      width_(0),
      height_(0),
      can_pause_(true),
      can_seek_forward_(true),
      can_seek_backward_(true),
      manager_(manager),
      cookie_getter_(cookie_getter),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)),
      listener_(base::MessageLoopProxy::current(),
                weak_this_.GetWeakPtr()) {}

MediaPlayerBridge::~MediaPlayerBridge() {
  Release();
}

void MediaPlayerBridge::InitializePlayer() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  j_media_player_class_.Reset(GetClass(env, "android/media/MediaPlayer"));

  jmethodID constructor = GetMethodID(env,
                                      j_media_player_class_,
                                      "<init>",
                                      "()V");
  ScopedJavaLocalRef<jobject> tmp(
      env, env->NewObject(j_media_player_class_.obj(), constructor));
  j_media_player_.Reset(tmp);

  jobject j_context = base::android::GetApplicationContext();
  DCHECK(j_context);

  listener_.CreateMediaPlayerListener(j_context, j_media_player_.obj());
}

void MediaPlayerBridge::SetVideoSurface(jobject surface) {
  if (j_media_player_.is_null() && surface != NULL)
    Prepare();

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 "setSurface",
                                 "(Landroid/view/Surface;)V");
  env->CallVoidMethod(j_media_player_.obj(), method, surface);
  CheckException(env);
}

void MediaPlayerBridge::Prepare() {
  if (j_media_player_.is_null())
    InitializePlayer();

  if (has_cookies_) {
    GetCookiesCallback(cookies_);
  } else {
    cookie_getter_->GetCookies(url_, first_party_for_cookies_, base::Bind(
        &MediaPlayerBridge::GetCookiesCallback, weak_this_.GetWeakPtr()));
  }
}

void MediaPlayerBridge::GetCookiesCallback(const std::string& cookies) {
  cookies_ = cookies;
  has_cookies_ = true;

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  // Create a Java String for the URL.
  ScopedJavaLocalRef<jstring> j_url_string = ConvertUTF8ToJavaString(env, url_);
  ScopedJavaLocalRef<jstring> j_cookies = ConvertUTF8ToJavaString(
      env, cookies_);

  jobject j_context = base::android::GetApplicationContext();
  DCHECK(j_context);

  if (Java_MediaPlayerBridge_setDataSource(
      env, j_media_player_.obj(), j_context, j_url_string.obj(),
      j_cookies.obj(), hide_url_log_)) {
    if (manager_)
      manager_->RequestMediaResources(this);
    CallVoidMethod("prepareAsync");
  } else {
    media_error_cb_.Run(player_id_, MEDIA_ERROR_UNKNOWN);
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

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env, j_media_player_class_, "isPlaying",
                                 "()Z");
  jboolean result = env->CallBooleanMethod(j_media_player_.obj(), method);
  CheckException(env);

  return result;
}

int MediaPlayerBridge::GetVideoWidth() {
  if (!prepared_)
    return width_;
  return CallIntMethod("getVideoWidth");
}

int MediaPlayerBridge::GetVideoHeight() {
  if (!prepared_)
    return height_;
  return CallIntMethod("getVideoHeight");
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
  return base::TimeDelta::FromMilliseconds(CallIntMethod("getCurrentPosition"));
}

base::TimeDelta MediaPlayerBridge::GetDuration() {
  if (!prepared_)
    return duration_;
  return base::TimeDelta::FromMilliseconds(CallIntMethod("getDuration"));
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
  CallVoidMethod("release");
  j_media_player_.Reset();
}

void MediaPlayerBridge::SetVolume(float left_volume, float right_volume) {
  if (j_media_player_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 "setVolume",
                                 "(FF)V");
  DCHECK(method);
  env->CallVoidMethod(j_media_player_.obj(), method,
                      left_volume, right_volume);
  CheckException(env);
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
  height_ = height_;
  video_size_changed_cb_.Run(player_id_, width, height);
}

void MediaPlayerBridge::OnBufferingUpdate(int percent) {
  buffering_update_cb_.Run(player_id_, percent);
}

void MediaPlayerBridge::OnPlaybackComplete() {
  time_update_timer_.Stop();
  playback_complete_cb_.Run(player_id_);
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
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 "getMetadata",
                                 "(ZZ)Landroid/media/Metadata;");
  ScopedJavaLocalRef<jobject> j_metadata(env,
      env->CallObjectMethod(j_media_player_.obj(),
      method, JNI_FALSE, JNI_FALSE));
  CheckException(env);
  if (j_metadata.is_null())
    return;

  ScopedJavaLocalRef<jclass> cls(GetClass(env, "android/media/Metadata"));
  jmethodID get_boolean = GetMethodID(env, cls, "getBoolean", "(I)Z");
  can_pause_ = env->CallBooleanMethod(j_metadata.obj(),
                                      get_boolean,
                                      kPauseAvailable);
  CheckException(env);
  can_seek_forward_ = env->CallBooleanMethod(j_metadata.obj(),
                                             get_boolean,
                                             kSeekBackwardAvailable);
  CheckException(env);
  can_seek_backward_ = env->CallBooleanMethod(j_metadata.obj(),
                                              get_boolean,
                                              kSeekForwardAvailable);
  CheckException(env);
}

void MediaPlayerBridge::StartInternal() {
  CallVoidMethod("start");
  if (!time_update_timer_.IsRunning()) {
    time_update_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kTimeUpdateInterval),
        this, &MediaPlayerBridge::DoTimeUpdate);
  }
}

void MediaPlayerBridge::PauseInternal() {
  CallVoidMethod("pause");
  time_update_timer_.Stop();
}

void MediaPlayerBridge::SeekInternal(base::TimeDelta time) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env, j_media_player_class_, "seekTo", "(I)V");
  DCHECK(method);
  int time_msec = static_cast<int>(time.InMilliseconds());
  DCHECK_EQ(time.InMilliseconds(), static_cast<int64>(time_msec));
  env->CallVoidMethod(j_media_player_.obj(),
                      method,
                      time_msec);
  CheckException(env);
}

// ---- JNI Helpers for repeated call patterns. ----

void MediaPlayerBridge::CallVoidMethod(std::string method_name) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 method_name.c_str(),
                                 "()V");
  env->CallVoidMethod(j_media_player_.obj(), method);
  CheckException(env);
}

int MediaPlayerBridge::CallIntMethod(std::string method_name) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env,
                                 j_media_player_class_,
                                 method_name.c_str(),
                                 "()I");
  jint j_result = env->CallIntMethod(j_media_player_.obj(), method);
  CheckException(env);
  return j_result;
}

bool MediaPlayerBridge::RegisterMediaPlayerBridge(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  DCHECK(g_MediaPlayerBridge_clazz);
  return ret;
}

}  // namespace media
