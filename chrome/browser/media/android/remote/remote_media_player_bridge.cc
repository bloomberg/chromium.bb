// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/remote/remote_media_player_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/media/android/remote/record_cast_action.h"
#include "chrome/browser/media/android/remote/remote_media_player_manager.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/RemoteMediaPlayerBridge_jni.h"
#include "media/base/android/media_common_android.h"
#include "media/base/android/media_resource_getter.h"
#include "media/base/timestamp_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using content::BrowserThread;

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
      local_player_(local_player),
      width_(0),
      height_(0),
      hide_url_log_(hide_url_log),
      url_(local_player->GetUrl()),
      first_party_for_cookies_(local_player->GetFirstPartyForCookies()),
      user_agent_(user_agent),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  java_bridge_.Reset(Java_RemoteMediaPlayerBridge_create(
      env, reinterpret_cast<intptr_t>(this), j_url_string.obj(),
      j_frame_url_string.obj(),
      ConvertUTF8ToJavaString(env, user_agent).obj()));
}

RemoteMediaPlayerBridge::~RemoteMediaPlayerBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  Java_RemoteMediaPlayerBridge_destroy(env, java_bridge_.obj());
  Release();
}

bool RemoteMediaPlayerBridge::HasVideo() const {
  NOTIMPLEMENTED();
  return true;
}

bool RemoteMediaPlayerBridge::HasAudio() const {
  NOTIMPLEMENTED();
  return true;
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

void RemoteMediaPlayerBridge::StartInternal() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  Java_RemoteMediaPlayerBridge_pause(env, java_bridge_.obj());
  time_update_timer_.Stop();
}

void RemoteMediaPlayerBridge::OnTimeUpdateTimerFired() {
  manager()->OnTimeUpdate(
      player_id(), GetCurrentTime(), base::TimeTicks::Now());
}

void RemoteMediaPlayerBridge::PauseLocal(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  local_player_->Pause(true);
  static_cast<RemoteMediaPlayerManager*>(manager())->OnPaused(player_id());
}

jint RemoteMediaPlayerBridge::GetLocalPosition(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  base::TimeDelta time = local_player_->GetCurrentTime();
  return static_cast<jint>(time.InMilliseconds());
}

void RemoteMediaPlayerBridge::OnCastStarting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& casting_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static_cast<RemoteMediaPlayerManager*>(manager())->SwitchToRemotePlayer(
      player_id(), ConvertJavaStringToUTF8(env, casting_message));
  if (!time_update_timer_.IsRunning()) {
    time_update_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(media::kTimeUpdateInterval), this,
        &RemoteMediaPlayerBridge::OnTimeUpdateTimerFired);
  }
}

void RemoteMediaPlayerBridge::OnCastStopping(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  static_cast<RemoteMediaPlayerManager*>(manager())
      ->SwitchToLocalPlayer(player_id());
}

void RemoteMediaPlayerBridge::OnSeekCompleted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  OnSeekComplete();
}

void RemoteMediaPlayerBridge::Pause(bool is_media_related_action) {
  // Ignore the pause if it's not from an event that is explicitly telling
  // the video to pause. It's possible for Pause() to be called for other
  // reasons, such as freeing resources, etc. and during those times, the
  // remote video playback should not be paused.
  if (is_media_related_action) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    JNIEnv* env = AttachCurrentThread();
    Java_RemoteMediaPlayerBridge_pause(env, java_bridge_.obj());
    time_update_timer_.Stop();
  }
}

void RemoteMediaPlayerBridge::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  // The surface is reset whenever the fullscreen view is destroyed or created.
  // Since the remote player doesn't use it, we forward it to the local player
  // for the time when user disconnects and resumes local playback
  // (see crbug.com/420690).
  local_player_->SetVideoSurface(std::move(surface));
}

void RemoteMediaPlayerBridge::OnPlaying(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  static_cast<RemoteMediaPlayerManager *>(manager())->OnPlaying(player_id());
}

void RemoteMediaPlayerBridge::OnPaused(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  static_cast<RemoteMediaPlayerManager *>(manager())->OnPaused(player_id());
}

void RemoteMediaPlayerBridge::OnRouteUnselected(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  casting_message_.reset();
  static_cast<RemoteMediaPlayerManager *>(manager())->OnRemoteDeviceUnselected(
      player_id());
}

void RemoteMediaPlayerBridge::OnPlaybackFinished(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  static_cast<RemoteMediaPlayerManager *>(manager())->OnRemotePlaybackFinished(
      player_id());
}

void RemoteMediaPlayerBridge::OnRouteAvailabilityChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_requestRemotePlayback(
      env, java_bridge_.obj(),
      local_player_->GetCurrentTime().InMilliseconds());
}

void RemoteMediaPlayerBridge::RequestRemotePlaybackControl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_requestRemotePlaybackControl(
      env, java_bridge_.obj());
}

void RemoteMediaPlayerBridge::SetNativePlayer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_setNativePlayer(
      env, java_bridge_.obj());
}

void RemoteMediaPlayerBridge::OnPlayerCreated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_onPlayerCreated(
      env, java_bridge_.obj());
}

void RemoteMediaPlayerBridge::OnPlayerDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_RemoteMediaPlayerBridge_onPlayerDestroyed(
      env, java_bridge_.obj());
}

std::string RemoteMediaPlayerBridge::GetCastingMessage() {
  return casting_message_ ?
      *casting_message_ : std::string();
}

void RemoteMediaPlayerBridge::SetPosterBitmap(
    const std::vector<SkBitmap>& bitmaps) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  StartInternal();
}

void RemoteMediaPlayerBridge::SeekTo(base::TimeDelta time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(aberent) Move the checks to the Java side.
  base::TimeDelta duration = GetDuration();

  if (time > duration)
    time = duration;

  // Seeking to an invalid position may cause media player to stuck in an
  // error state.
  if (time < base::TimeDelta()) {
    DCHECK_EQ(-1.0, time.InMillisecondsF());
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  int time_msec = static_cast<int>(time.InMilliseconds());
  Java_RemoteMediaPlayerBridge_seekTo(env, java_bridge_.obj(), time_msec);
}

void RemoteMediaPlayerBridge::Release() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  time_update_timer_.Stop();
  JNIEnv* env = AttachCurrentThread();
  Java_RemoteMediaPlayerBridge_release(env, java_bridge_.obj());
  DetachListener();
}

void RemoteMediaPlayerBridge::UpdateEffectiveVolumeInternal(
    double effective_volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  Java_RemoteMediaPlayerBridge_setVolume(
      env, java_bridge_.obj(), GetEffectiveVolume());
}

base::TimeDelta RemoteMediaPlayerBridge::GetCurrentTime() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  return base::TimeDelta::FromMilliseconds(
      Java_RemoteMediaPlayerBridge_getCurrentPosition(
          env, java_bridge_.obj()));
}

base::TimeDelta RemoteMediaPlayerBridge::GetDuration() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  return true;
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

base::android::ScopedJavaLocalRef<jstring> RemoteMediaPlayerBridge::GetTitle(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
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

void RemoteMediaPlayerBridge::OnError(
    JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
      // TODO(https://crbug.com/585379) implement some useful codes for remote
      // playback. None of the existing MediaPlayerAndroid codes are
      // relevant for remote playback.
      manager()->OnError(player_id(), MEDIA_ERROR_INVALID_CODE);
}


void RemoteMediaPlayerBridge::OnCookiesRetrieved(const std::string& cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(aberent) Do we need to retrieve auth credentials for basic
  // authentication? MediaPlayerBridge does.
  cookies_ = cookies;
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  Java_RemoteMediaPlayerBridge_setCookies(
      env, java_bridge_.obj(), ConvertUTF8ToJavaString(env, cookies).obj());
}

}  // namespace remote_media
