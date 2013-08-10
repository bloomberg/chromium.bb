// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "jni/MediaPlayerBridge_jni.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/media_resource_getter.h"
#include "media/base/android/media_source_player.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

// Time update happens every 250ms.
static const int kTimeUpdateInterval = 250;

// Android MediaMetadataRetriever may fail to extract the metadata from the
// media under some circumstances. This makes the user unable to perform
// seek. To solve this problem, we use a temporary duration of 100 seconds when
// the duration is unknown. And we scale the seek position later when duration
// is available.
static const int kTemporaryDuration = 100;

namespace media {

#if !defined(GOOGLE_TV)
// static
MediaPlayerAndroid* MediaPlayerAndroid::Create(
    int player_id,
    const GURL& url,
    SourceType source_type,
    const GURL& first_party_for_cookies,
    bool hide_url_log,
    MediaPlayerManager* manager) {
  if (source_type == SOURCE_TYPE_URL) {
    MediaPlayerBridge* media_player_bridge = new MediaPlayerBridge(
        player_id,
        url,
        first_party_for_cookies,
        hide_url_log,
        manager);
    media_player_bridge->Initialize();
    return media_player_bridge;
  } else {
    return new MediaSourcePlayer(
        player_id,
        manager);
  }
}
#endif

MediaPlayerBridge::MediaPlayerBridge(
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool hide_url_log,
    MediaPlayerManager* manager)
    : MediaPlayerAndroid(player_id,
                         manager),
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
      weak_this_(this),
      listener_(base::MessageLoopProxy::current(),
                weak_this_.GetWeakPtr()) {
}

MediaPlayerBridge::~MediaPlayerBridge() {
  Release();
}

void MediaPlayerBridge::Initialize() {
  if (url_.SchemeIsFile()) {
    cookies_.clear();
    ExtractMediaMetadata(url_.spec());
    return;
  }

  media::MediaResourceGetter* resource_getter =
      manager()->GetMediaResourceGetter();
  if (url_.SchemeIsFileSystem()) {
    cookies_.clear();
    resource_getter->GetPlatformPathFromFileSystemURL(url_, base::Bind(
        &MediaPlayerBridge::ExtractMediaMetadata, weak_this_.GetWeakPtr()));
    return;
  }

  resource_getter->GetCookies(url_, first_party_for_cookies_, base::Bind(
      &MediaPlayerBridge::OnCookiesRetrieved, weak_this_.GetWeakPtr()));
}

void MediaPlayerBridge::CreateJavaMediaPlayerBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  j_media_player_bridge_.Reset(Java_MediaPlayerBridge_create(env));

  SetMediaPlayerListener();
}

void MediaPlayerBridge::SetJavaMediaPlayerBridge(
    jobject j_media_player_bridge) {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  j_media_player_bridge_.Reset(env, j_media_player_bridge);
}

void MediaPlayerBridge::SetMediaPlayerListener() {
  jobject j_context = base::android::GetApplicationContext();
  DCHECK(j_context);

  listener_.CreateMediaPlayerListener(j_context, j_media_player_bridge_.obj());
}

void MediaPlayerBridge::SetDuration(base::TimeDelta duration) {
  duration_ = duration;
}

void MediaPlayerBridge::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  if (j_media_player_bridge_.is_null()) {
    if (surface.IsEmpty())
      return;
    Prepare();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  Java_MediaPlayerBridge_setSurface(
      env, j_media_player_bridge_.obj(), surface.j_surface().obj());
}

void MediaPlayerBridge::Prepare() {
  if (j_media_player_bridge_.is_null())
    CreateJavaMediaPlayerBridge();
  if (url_.SchemeIsFileSystem()) {
    manager()->GetMediaResourceGetter()->GetPlatformPathFromFileSystemURL(
            url_, base::Bind(&MediaPlayerBridge::SetDataSource,
                             weak_this_.GetWeakPtr()));
  } else {
    SetDataSource(url_.spec());
  }
}

void MediaPlayerBridge::SetDataSource(const std::string& url) {
  if (j_media_player_bridge_.is_null())
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
      env, j_media_player_bridge_.obj(), j_context, j_url_string.obj(),
      j_cookies.obj(), hide_url_log_)) {
    RequestMediaResourcesFromManager();
    Java_MediaPlayerBridge_prepareAsync(
        env, j_media_player_bridge_.obj());
  } else {
    OnMediaError(MEDIA_ERROR_FORMAT);
  }
}

void MediaPlayerBridge::OnCookiesRetrieved(const std::string& cookies) {
  cookies_ = cookies;
  ExtractMediaMetadata(url_.spec());
}

void MediaPlayerBridge::ExtractMediaMetadata(const std::string& url) {
  manager()->GetMediaResourceGetter()->ExtractMediaMetadata(
      url, cookies_, base::Bind(&MediaPlayerBridge::OnMediaMetadataExtracted,
                                weak_this_.GetWeakPtr()));
}

void MediaPlayerBridge::OnMediaMetadataExtracted(
    base::TimeDelta duration, int width, int height, bool success) {
  if (success) {
    duration_ = duration;
    width_ = width;
    height_ = height;
  }
  OnMediaMetadataChanged(duration_, width_, height_, success);
}

void MediaPlayerBridge::Start() {
  if (j_media_player_bridge_.is_null()) {
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
  if (j_media_player_bridge_.is_null()) {
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
  jboolean result = Java_MediaPlayerBridge_isPlaying(
      env, j_media_player_bridge_.obj());
  return result;
}

int MediaPlayerBridge::GetVideoWidth() {
  if (!prepared_)
    return width_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MediaPlayerBridge_getVideoWidth(
      env, j_media_player_bridge_.obj());
}

int MediaPlayerBridge::GetVideoHeight() {
  if (!prepared_)
    return height_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MediaPlayerBridge_getVideoHeight(
      env, j_media_player_bridge_.obj());
}

void MediaPlayerBridge::SeekTo(base::TimeDelta time) {
  // Record the time to seek when OnMediaPrepared() is called.
  pending_seek_ = time;

  if (j_media_player_bridge_.is_null())
    Prepare();
  else if (prepared_)
    SeekInternal(time);
}

base::TimeDelta MediaPlayerBridge::GetCurrentTime() {
  if (!prepared_)
    return pending_seek_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::TimeDelta::FromMilliseconds(
      Java_MediaPlayerBridge_getCurrentPosition(
          env, j_media_player_bridge_.obj()));
}

base::TimeDelta MediaPlayerBridge::GetDuration() {
  if (!prepared_)
    return duration_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::TimeDelta::FromMilliseconds(
      Java_MediaPlayerBridge_getDuration(
          env, j_media_player_bridge_.obj()));
}

void MediaPlayerBridge::Release() {
  if (j_media_player_bridge_.is_null())
    return;

  time_update_timer_.Stop();
  if (prepared_)
    pending_seek_ = GetCurrentTime();
  prepared_ = false;
  pending_play_ = false;
  SetVideoSurface(gfx::ScopedJavaSurface());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_release(env, j_media_player_bridge_.obj());
  j_media_player_bridge_.Reset();
  ReleaseMediaResourcesFromManager();
  listener_.ReleaseMediaPlayerListenerResources();
}

void MediaPlayerBridge::SetVolume(double volume) {
  if (j_media_player_bridge_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  Java_MediaPlayerBridge_setVolume(
      env, j_media_player_bridge_.obj(), volume);
}

void MediaPlayerBridge::OnVideoSizeChanged(int width, int height) {
  width_ = width;
  height_ = height;
  MediaPlayerAndroid::OnVideoSizeChanged(width, height);
}

void MediaPlayerBridge::OnPlaybackComplete() {
  time_update_timer_.Stop();
  MediaPlayerAndroid::OnPlaybackComplete();
}

void MediaPlayerBridge::OnMediaInterrupted() {
  time_update_timer_.Stop();
  MediaPlayerAndroid::OnMediaInterrupted();
}

void MediaPlayerBridge::OnMediaPrepared() {
  if (j_media_player_bridge_.is_null())
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
  PendingSeekInternal(pending_seek_);

  if (pending_play_) {
    StartInternal();
    pending_play_ = false;
  }

  GetAllowedOperations();
  OnMediaMetadataChanged(duration_, width_, height_, true);
}

void MediaPlayerBridge::GetAllowedOperations() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jobject> allowedOperations =
      Java_MediaPlayerBridge_getAllowedOperations(
          env, j_media_player_bridge_.obj());
  can_pause_ = Java_AllowedOperations_canPause(env, allowedOperations.obj());
  can_seek_forward_ = Java_AllowedOperations_canSeekForward(
      env, allowedOperations.obj());
  can_seek_backward_ = Java_AllowedOperations_canSeekBackward(
      env, allowedOperations.obj());
}

void MediaPlayerBridge::StartInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_start(env, j_media_player_bridge_.obj());
  if (!time_update_timer_.IsRunning()) {
    time_update_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kTimeUpdateInterval),
        this, &MediaPlayerBridge::OnTimeUpdated);
  }
}

void MediaPlayerBridge::PauseInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_pause(env, j_media_player_bridge_.obj());
  time_update_timer_.Stop();
}

void MediaPlayerBridge::PendingSeekInternal(base::TimeDelta time) {
  SeekInternal(time);
}

void MediaPlayerBridge::SeekInternal(base::TimeDelta time) {
  if (time > duration_)
    time = duration_;

  // Seeking to an invalid position may cause media player to stuck in an
  // error state.
  if (time < base::TimeDelta()) {
    DCHECK_EQ(-1.0, time.InMillisecondsF());
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  int time_msec = static_cast<int>(time.InMilliseconds());
  Java_MediaPlayerBridge_seekTo(
      env, j_media_player_bridge_.obj(), time_msec);
}

bool MediaPlayerBridge::RegisterMediaPlayerBridge(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  DCHECK(g_MediaPlayerBridge_clazz);
  return ret;
}

bool MediaPlayerBridge::CanPause() {
  return can_pause_;
}

bool MediaPlayerBridge::CanSeekForward() {
  return can_seek_forward_;
}

bool MediaPlayerBridge::CanSeekBackward() {
  return can_seek_backward_;
}

bool MediaPlayerBridge::IsPlayerReady() {
  return prepared_;
}

GURL MediaPlayerBridge::GetUrl() {
  return url_;
}

GURL MediaPlayerBridge::GetFirstPartyForCookies() {
  return first_party_for_cookies_;
}

}  // namespace media
