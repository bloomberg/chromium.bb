// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_video_view.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/power_save_blocker_impl.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "jni/ContentVideoView_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaGlobalRef;
using base::UserMetricsAction;
using content::RecordAction;

namespace content {

namespace {
// There can only be one content video view at a time, this holds onto that
// singleton instance.
ContentVideoView* g_content_video_view = NULL;

}  // namespace

static jobject GetSingletonJavaContentVideoView(JNIEnv*env, jclass) {
  if (g_content_video_view)
    return g_content_video_view->GetJavaObject(env).Release();
  else
    return NULL;
}

bool ContentVideoView::RegisterContentVideoView(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ContentVideoView* ContentVideoView::GetInstance() {
  return g_content_video_view;
}

ContentVideoView::ContentVideoView(
    BrowserMediaPlayerManager* manager)
    : manager_(manager),
      weak_factory_(this) {
  DCHECK(!g_content_video_view);
  j_content_video_view_ = CreateJavaObject();
  g_content_video_view = this;
  CreatePowerSaveBlocker();
}

ContentVideoView::~ContentVideoView() {
  DCHECK(g_content_video_view);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_ContentVideoView_destroyContentVideoView(env,
        content_video_view.obj(), true);
    j_content_video_view_.reset();
  }
  g_content_video_view = NULL;
}

void ContentVideoView::OpenVideo() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    CreatePowerSaveBlocker();
    Java_ContentVideoView_openVideo(env, content_video_view.obj());
  }
}

void ContentVideoView::OnMediaPlayerError(int error_type) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    power_save_blocker_.reset();
    Java_ContentVideoView_onMediaPlayerError(env, content_video_view.obj(),
        error_type);
  }
}

void ContentVideoView::OnVideoSizeChanged(int width, int height) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_ContentVideoView_onVideoSizeChanged(env, content_video_view.obj(),
        width, height);
  }
}

void ContentVideoView::OnBufferingUpdate(int percent) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_ContentVideoView_onBufferingUpdate(env, content_video_view.obj(),
        percent);
  }
}

void ContentVideoView::OnPlaybackComplete() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    power_save_blocker_.reset();
    Java_ContentVideoView_onPlaybackComplete(env, content_video_view.obj());
  }
}

void ContentVideoView::OnExitFullscreen() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null())
    Java_ContentVideoView_onExitFullscreen(env, content_video_view.obj());
}

void ContentVideoView::RecordFullscreenPlayback(
    JNIEnv*, jobject, bool is_portrait_video, bool is_orientation_portrait) {
  UMA_HISTOGRAM_BOOLEAN("MobileFullscreenVideo.OrientationPortrait",
                        is_orientation_portrait);
  UMA_HISTOGRAM_BOOLEAN("MobileFullscreenVideo.VideoPortrait",
                        is_portrait_video);
}

void ContentVideoView::RecordExitFullscreenPlayback(
    JNIEnv*, jobject, bool is_portrait_video,
    long playback_duration_in_milliseconds_before_orientation_change,
    long playback_duration_in_milliseconds_after_orientation_change) {
  bool orientation_changed = (
      playback_duration_in_milliseconds_after_orientation_change != 0);
  if (is_portrait_video) {
    UMA_HISTOGRAM_COUNTS(
        "MobileFullscreenVideo.PortraitDuration",
        playback_duration_in_milliseconds_before_orientation_change);
    UMA_HISTOGRAM_COUNTS(
        "MobileFullscreenVideo.PortraitRotation", orientation_changed);
    if (orientation_changed) {
      UMA_HISTOGRAM_COUNTS(
          "MobileFullscreenVideo.DurationAfterPotraitRotation",
          playback_duration_in_milliseconds_after_orientation_change);
    }
  } else {
    UMA_HISTOGRAM_COUNTS(
        "MobileFullscreenVideo.LandscapeDuration",
        playback_duration_in_milliseconds_before_orientation_change);
    UMA_HISTOGRAM_COUNTS(
        "MobileFullscreenVideo.LandscapeRotation", orientation_changed);
  }
}

void ContentVideoView::UpdateMediaMetadata() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (content_video_view.is_null())
    return;

  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  if (player && player->IsPlayerReady()) {
    Java_ContentVideoView_onUpdateMediaMetadata(
        env, content_video_view.obj(), player->GetVideoWidth(),
        player->GetVideoHeight(),
        static_cast<int>(player->GetDuration().InMilliseconds()),
        player->CanPause(),player->CanSeekForward(), player->CanSeekBackward());
  }
}

int ContentVideoView::GetVideoWidth(JNIEnv*, jobject obj) const {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->GetVideoWidth() : 0;
}

int ContentVideoView::GetVideoHeight(JNIEnv*, jobject obj) const {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->GetVideoHeight() : 0;
}

int ContentVideoView::GetDurationInMilliSeconds(JNIEnv*, jobject obj) const {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->GetDuration().InMilliseconds() : -1;
}

int ContentVideoView::GetCurrentPosition(JNIEnv*, jobject obj) const {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->GetCurrentTime().InMilliseconds() : 0;
}

bool ContentVideoView::IsPlaying(JNIEnv*, jobject obj) {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->IsPlaying() : false;
}

void ContentVideoView::SeekTo(JNIEnv*, jobject obj, jint msec) {
  manager_->FullscreenPlayerSeek(msec);
}

void ContentVideoView::Play(JNIEnv*, jobject obj) {
  CreatePowerSaveBlocker();
  manager_->FullscreenPlayerPlay();
}

void ContentVideoView::Pause(JNIEnv*, jobject obj) {
  power_save_blocker_.reset();
  manager_->FullscreenPlayerPause();
}

void ContentVideoView::ExitFullscreen(
    JNIEnv*, jobject, jboolean release_media_player) {
  power_save_blocker_.reset();
  j_content_video_view_.reset();
  manager_->ExitFullscreen(release_media_player);
}

void ContentVideoView::SetSurface(JNIEnv* env, jobject obj,
                                  jobject surface) {
  manager_->SetVideoSurface(
      gfx::ScopedJavaSurface::AcquireExternalSurface(surface));
}

void ContentVideoView::RequestMediaMetadata(JNIEnv* env, jobject obj) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ContentVideoView::UpdateMediaMetadata,
                 weak_factory_.GetWeakPtr()));
}

ScopedJavaLocalRef<jobject> ContentVideoView::GetJavaObject(JNIEnv* env) {
  return j_content_video_view_.get(env);
}

gfx::NativeView ContentVideoView::GetNativeView() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (content_video_view.is_null())
    return NULL;

  return reinterpret_cast<gfx::NativeView>(
      Java_ContentVideoView_getNativeViewAndroid(env,
                                                 content_video_view.obj()));

}

JavaObjectWeakGlobalRef ContentVideoView::CreateJavaObject() {
  ContentViewCoreImpl* content_view_core = manager_->GetContentViewCore();
  JNIEnv* env = AttachCurrentThread();
  return JavaObjectWeakGlobalRef(
      env,
      Java_ContentVideoView_createContentVideoView(
          env,
          content_view_core->GetContext().obj(),
          reinterpret_cast<intptr_t>(this),
          content_view_core->GetContentVideoViewClient().obj()).obj());
}

void ContentVideoView::CreatePowerSaveBlocker() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableContentVideoViewPowerSaveBlocker)) {
    // In fullscreen Clank reuses the power save blocker attached to the
    // container view that was created for embedded video. The WebView cannot
    // reuse that so we create a new blocker instead.
    if (power_save_blocker_) return;
    power_save_blocker_ = PowerSaveBlocker::Create(
        PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
        "Playing video").Pass();
    static_cast<PowerSaveBlockerImpl*>(power_save_blocker_.get())->
        InitDisplaySleepBlocker(GetNativeView());
  }
}

}  // namespace content
