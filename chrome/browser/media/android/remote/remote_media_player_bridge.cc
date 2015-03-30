// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/remote/remote_media_player_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/media/android/remote/record_cast_action.h"
#include "chrome/browser/media/android/remote/remote_media_player_manager.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/RemoteMediaPlayerBridge_jni.h"
#include "media/base/android/media_common_android.h"
#include "media/base/android/media_resource_getter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace {
/*
 * Dummy function for RequestMediaResources callback. The callback is never
 * actually called by MediaPlayerAndroid or RemoteMediaPlayer but is needed
 * to compile the constructor call.
 */
void DoNothing(int /*i*/) {}
}

namespace remote_media {

RemoteMediaPlayerBridge::RemoteMediaPlayerBridge(
    MediaPlayerAndroid* local_player, const std::string& user_agent,
    bool hide_url_log, RemoteMediaPlayerManager* manager)
    : MediaPlayerAndroid(local_player->player_id(), manager,
                         base::Bind(&DoNothing),
                         local_player->frame_url()),
      start_position_millis_(0),
      local_player_(local_player),
      in_use_(false),
      prepared_(false),
      pending_play_(false),
      width_(0),
      height_(0),
      should_seek_on_prepare_(false),
      hide_url_log_(hide_url_log),
      volume_(-1.0),
      url_(local_player->GetUrl()),
      first_party_for_cookies_(local_player->GetFirstPartyForCookies()),
      user_agent_(user_agent),
      weak_factory_(this) {
  if (local_player->GetCurrentTime().InMilliseconds() > 0)
    start_position_millis_ = local_player->GetCurrentTime().InMilliseconds();
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jstring> j_url_string;
  if (url_.is_valid()) {
    // Create a Java String for the URL.
    j_url_string = ConvertUTF8ToJavaString(env, url_.spec());
  }
  ScopedJavaLocalRef<jstring> j_frame_url_string;
  if (local_player->frame_url().is_valid()) {
    // Create a Java String for the URL.
    j_frame_url_string = ConvertUTF8ToJavaString(
        env, local_player->frame_url().spec());
  }
  java_bridge_.Reset(
      Java_RemoteMediaPlayerBridge_create(env, reinterpret_cast<intptr_t>(this),
                                          start_position_millis_,
                                          j_url_string.obj(),
                                          j_frame_url_string.obj()));
}

RemoteMediaPlayerBridge::~RemoteMediaPlayerBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  Java_RemoteMediaPlayerBridge_destroy(env, java_bridge_.obj());
  Release();
}

int RemoteMediaPlayerBridge::GetVideoWidth() {
  return local_player_->GetVideoWidth();
}

int RemoteMediaPlayerBridge::GetVideoHeight() {
  return local_player_->GetVideoHeight();
}

void RemoteMediaPlayerBridge::OnVideoSizeChanged(int width, int height) {
  width_ = width;
  height_ = height;
  MediaPlayerAndroid::OnVideoSizeChanged(width, height);
}

void RemoteMediaPlayerBridge::OnPlaybackComplete() {
  time_update_timer_.Stop();
  MediaPlayerAndroid::OnPlaybackComplete();
}

void RemoteMediaPlayerBridge::OnMediaInterrupted() {}

void RemoteMediaPlayerBridge::OnMediaPrepared() {
  if (!in_use_)
    return;

  prepared_ = true;
  duration_ = GetDuration();

  // If media player was recovered from a saved state, consume all the pending
  // events.
  if (should_seek_on_prepare_) {
    PendingSeekInternal(pending_seek_);
    pending_seek_ = base::TimeDelta::FromMilliseconds(0);
    should_seek_on_prepare_ = false;
  }

  if (pending_play_) {
    StartInternal();
    pending_play_ = false;
  }

  manager()->OnMediaMetadataChanged(
      player_id(), duration_, width_, height_, true);
}

void RemoteMediaPlayerBridge::StartInternal() {
  JNIEnv* env = AttachCurrentThread();
  Java_RemoteMediaPlayerBridge_start(env, java_bridge_.obj());
  if (!time_update_timer_.IsRunning()) {
    time_update_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(media::kTimeUpdateInterval),
        this, &RemoteMediaPlayerBridge::OnTimeUpdateTimerFired);
  }
}

void RemoteMediaPlayerBridge::PauseInternal() {
  JNIEnv* env = AttachCurrentThread();
  Java_RemoteMediaPlayerBridge_pause(env, java_bridge_.obj());
  time_update_timer_.Stop();
}

void RemoteMediaPlayerBridge::SeekInternal(base::TimeDelta time) {
  if (time > duration_)
    time = duration_;

  // Seeking to an invalid position may cause media player to stuck in an
  // error state.
  if (time < base::TimeDelta()) {
    DCHECK_EQ(-1.0, time.InMillisecondsF());
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  int time_msec = static_cast<int>(time.InMilliseconds());
  Java_RemoteMediaPlayerBridge_seekTo(
      env, java_bridge_.obj(), time_msec);
}

void RemoteMediaPlayerBridge::OnTimeUpdateTimerFired() {
  manager()->OnTimeUpdate(
      player_id(), GetCurrentTime(), base::TimeTicks::Now());
}

void RemoteMediaPlayerBridge::PendingSeekInternal(const base::TimeDelta& time) {
  SeekInternal(time);
}

void RemoteMediaPlayerBridge::Prepare() {
  DCHECK(!in_use_);
  DCHECK(IsMediaPlayableRemotely());
  in_use_ = true;
  AttachListener(java_bridge_.obj());
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  if (url_.is_valid()) {
    // Create a Java String for the URL.
    ScopedJavaLocalRef<jstring> j_url_string =
        ConvertUTF8ToJavaString(env, url_.spec());

    jobject j_context = base::android::GetApplicationContext();
    DCHECK(j_context);

    ScopedJavaLocalRef<jstring> j_cookies = ConvertUTF8ToJavaString(
        env, cookies_);
    ScopedJavaLocalRef<jstring> j_user_agent = ConvertUTF8ToJavaString(
        env, user_agent_);

    if (!Java_RemoteMediaPlayerBridge_setDataSource(
        env, java_bridge_.obj(), j_context, j_url_string.obj(),
        j_cookies.obj(), j_user_agent.obj(), hide_url_log_)) {
      OnMediaError(MEDIA_ERROR_FORMAT);
      return;
    }
  }

  if (!Java_RemoteMediaPlayerBridge_prepareAsync(env, java_bridge_.obj()))
    OnMediaError(MEDIA_ERROR_FORMAT);
}

void RemoteMediaPlayerBridge::Pause(bool is_media_related_action) {
  // Ignore the pause if it's not from an event that is explicitly telling
  // the video to pause. It's possible for Pause() to be called for other
  // reasons, such as freeing resources, etc. and during those times, the
  // remote video playback should not be paused.
  if (is_media_related_action) {
    if (!in_use_) {
      pending_play_ = false;
    } else {
      if (prepared_ && IsPlaying())
        PauseInternal();
      else
        pending_play_ = false;
    }
  }
}

void RemoteMediaPlayerBridge::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  // The surface is reset whenever the fullscreen view is destroyed or created.
  // Since the remote player doesn't use it, we forward it to the local player
  // for the time when user disconnects and resumes local playback
  // (see crbug.com/420690).
  local_player_->SetVideoSurface(surface.Pass());
}

base::android::ScopedJavaLocalRef<jstring> RemoteMediaPlayerBridge::GetFrameUrl(
    JNIEnv* env, jobject obj) {
  return ConvertUTF8ToJavaString(env, frame_url().spec());
}

void RemoteMediaPlayerBridge::OnPlaying(JNIEnv* env, jobject obj) {
  static_cast<RemoteMediaPlayerManager *>(manager())->OnPlaying(player_id());
}

void RemoteMediaPlayerBridge::OnPaused(JNIEnv* env, jobject obj) {
  static_cast<RemoteMediaPlayerManager *>(manager())->OnPaused(player_id());
}

void RemoteMediaPlayerBridge::OnRouteSelected(JNIEnv* env, jobject obj,
                                              jstring castingMessage) {
  casting_message_.reset(
      new std::string(
          base::android::ConvertJavaStringToUTF8(env, castingMessage)));
  static_cast<RemoteMediaPlayerManager *>(manager())->OnRemoteDeviceSelected(
      player_id());
}

void RemoteMediaPlayerBridge::OnRouteUnselected(JNIEnv* env, jobject obj) {
  casting_message_.reset();
  static_cast<RemoteMediaPlayerManager *>(manager())->OnRemoteDeviceUnselected(
      player_id());
}

void RemoteMediaPlayerBridge::OnPlaybackFinished(JNIEnv* env, jobject obj) {
  static_cast<RemoteMediaPlayerManager *>(manager())->OnRemotePlaybackFinished(
      player_id());
}

void RemoteMediaPlayerBridge::OnRouteAvailabilityChanged(JNIEnv* env,
                                                         jobject obj,
                                                         jboolean available) {
  static_cast<RemoteMediaPlayerManager *>(manager())->
      OnRouteAvailabilityChanged(player_id(), available);
}

// static
bool RemoteMediaPlayerBridge::RegisterRemoteMediaPlayerBridge(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  DCHECK(g_RemoteMediaPlayerBridge_clazz);
  return ret;
}

void RemoteMediaPlayerBridge::RequestRemotePlayback() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_requestRemotePlayback(
      env, java_bridge_.obj());
}

void RemoteMediaPlayerBridge::RequestRemotePlaybackControl() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_requestRemotePlaybackControl(
      env, java_bridge_.obj());
}

void RemoteMediaPlayerBridge::SetNativePlayer() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_setNativePlayer(
      env, java_bridge_.obj());
}

void RemoteMediaPlayerBridge::OnPlayerCreated() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_onPlayerCreated(
      env, java_bridge_.obj());
}

void RemoteMediaPlayerBridge::OnPlayerDestroyed() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_onPlayerDestroyed(
      env, java_bridge_.obj());
}

bool RemoteMediaPlayerBridge::IsRemotePlaybackAvailable() const {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jboolean result = Java_RemoteMediaPlayerBridge_isRemotePlaybackAvailable(
      env, java_bridge_.obj());

  return result;
}

bool RemoteMediaPlayerBridge::IsRemotePlaybackPreferredForFrame() const {
  if (in_use_) {
    // We have already decided to use remote playback
    return true;
  }
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jboolean result =
      Java_RemoteMediaPlayerBridge_isRemotePlaybackPreferredForFrame(
          env, java_bridge_.obj());
  return result;
}

std::string RemoteMediaPlayerBridge::GetCastingMessage() {
  return casting_message_ ?
      *casting_message_ : std::string();
}

void RemoteMediaPlayerBridge::SetPosterBitmap(
    const std::vector<SkBitmap>& bitmaps) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  if (bitmaps.empty()) {
    Java_RemoteMediaPlayerBridge_setPosterBitmap(env, java_bridge_.obj(), NULL);
  } else {
    ScopedJavaLocalRef<jobject> j_poster_bitmap;
    j_poster_bitmap = gfx::ConvertToJavaBitmap(&(bitmaps[0]));

    Java_RemoteMediaPlayerBridge_setPosterBitmap(env, java_bridge_.obj(),
                                                 j_poster_bitmap.obj());
  }
}

void RemoteMediaPlayerBridge::Start() {
  if (!in_use_) {
    pending_play_ = true;
    Prepare();
  } else {
    if (prepared_)
      StartInternal();
    else
      pending_play_ = true;
  }
}

void RemoteMediaPlayerBridge::SeekTo(base::TimeDelta timestamp) {
  // Record the time to seek when OnMediaPrepared() is called.
  pending_seek_ = timestamp;
  should_seek_on_prepare_ = true;

  if (!in_use_)
    Prepare();
  else if (prepared_)
    SeekInternal(timestamp);
}

void RemoteMediaPlayerBridge::Release() {
  if (!in_use_)
    return;
  time_update_timer_.Stop();
  if (prepared_) {
    pending_seek_ = GetCurrentTime();
    should_seek_on_prepare_ = true;
  }

  prepared_ = false;
  pending_play_ = false;
  JNIEnv* env = AttachCurrentThread();
  Java_RemoteMediaPlayerBridge_release(env, java_bridge_.obj());
  DetachListener();
  in_use_ = false;
}

void RemoteMediaPlayerBridge::SetVolume(double volume) {
  if (!in_use_) {
    volume_ = volume;
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  Java_RemoteMediaPlayerBridge_setVolume(
      env, java_bridge_.obj(), volume);
}

base::TimeDelta RemoteMediaPlayerBridge::GetCurrentTime() {
  if (!prepared_)
    return pending_seek_;
  JNIEnv* env = AttachCurrentThread();
  return base::TimeDelta::FromMilliseconds(
      Java_RemoteMediaPlayerBridge_getCurrentPosition(
          env, java_bridge_.obj()));
}

base::TimeDelta RemoteMediaPlayerBridge::GetDuration() {
  if (!prepared_)
    return duration_;
  JNIEnv* env = AttachCurrentThread();
  const int duration_ms =
      Java_RemoteMediaPlayerBridge_getDuration(env, java_bridge_.obj());
  // Sometimes we can't get the duration remotely, but the local media player
  // knows it.
  // TODO (aberent) This is for YouTube. Remove it when the YouTube receiver is
  // fixed.
  if (duration_ms == 0) {
    return local_player_->GetDuration();
  }
  return duration_ms < 0 ? media::kInfiniteDuration()
                         : base::TimeDelta::FromMilliseconds(duration_ms);
}

bool RemoteMediaPlayerBridge::IsPlaying() {
  if (!prepared_)
    return pending_play_;

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  jboolean result = Java_RemoteMediaPlayerBridge_isPlaying(
      env, java_bridge_.obj());
  return result;
}

bool RemoteMediaPlayerBridge::CanPause() {
  return true;
}

bool RemoteMediaPlayerBridge::CanSeekForward() {
  return true;
}

bool RemoteMediaPlayerBridge::CanSeekBackward() {
  return true;
}

bool RemoteMediaPlayerBridge::IsPlayerReady() {
  return prepared_;
}

GURL RemoteMediaPlayerBridge::GetUrl() {
  return url_;
}

GURL RemoteMediaPlayerBridge::GetFirstPartyForCookies() {
  return first_party_for_cookies_;
}

void RemoteMediaPlayerBridge::Initialize() {
  cookies_.clear();
  media::MediaResourceGetter* resource_getter =
      manager()->GetMediaResourceGetter();
  resource_getter->GetCookies(
      url_, first_party_for_cookies_,
      base::Bind(&RemoteMediaPlayerBridge::OnCookiesRetrieved,
                 weak_factory_.GetWeakPtr()));
}

bool RemoteMediaPlayerBridge::IsMediaPlayableRemotely() const {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  return Java_RemoteMediaPlayerBridge_isMediaPlayableRemotely(
      env, java_bridge_.obj());
}

base::android::ScopedJavaLocalRef<jstring> RemoteMediaPlayerBridge::GetTitle(
    JNIEnv* env, jobject obj) {
  base::string16 title;
  content::ContentViewCore* core =
      static_cast<RemoteMediaPlayerManager*>(manager())->GetContentViewCore();
  if (core) {
    content::WebContents* contents = core->GetWebContents();
    if (contents) {
      title = contents->GetTitle();
    }
  }
  return base::android::ConvertUTF16ToJavaString(env, title);
}

void RemoteMediaPlayerBridge::OnCookiesRetrieved(const std::string& cookies) {
  // TODO(aberent) Do we need to retrieve auth credentials for basic
  // authentication? MediaPlayerBridge does.
  cookies_ = cookies;
}

} // namespace remote_media
