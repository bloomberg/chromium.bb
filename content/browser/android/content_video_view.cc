// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_video_view.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/android/browser_media_player_manager.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/public/common/content_switches.h"
#include "jni/ContentVideoView_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaGlobalRef;

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

bool ContentVideoView::HasContentVideoView() {
  return g_content_video_view;
}

ContentVideoView::ContentVideoView(
    const ScopedJavaLocalRef<jobject>& context,
    const ScopedJavaLocalRef<jobject>& client,
    BrowserMediaPlayerManager* manager)
    : manager_(manager) {
  DCHECK(!g_content_video_view);
  JNIEnv *env = AttachCurrentThread();
  j_content_video_view_ = JavaObjectWeakGlobalRef(env,
      Java_ContentVideoView_createContentVideoView(env, context.obj(),
          reinterpret_cast<int>(this), client.obj()).obj());
  g_content_video_view = this;
}

ContentVideoView::~ContentVideoView() {
  DCHECK(g_content_video_view);
  DestroyContentVideoView(true);
  g_content_video_view = NULL;
}

void ContentVideoView::OpenVideo() {
  JNIEnv *env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null())
    Java_ContentVideoView_openVideo(env, content_video_view.obj());
}

void ContentVideoView::OnMediaPlayerError(int error_type) {
  JNIEnv *env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_ContentVideoView_onMediaPlayerError(env, content_video_view.obj(),
        error_type);
  }
}

void ContentVideoView::OnVideoSizeChanged(int width, int height) {
  JNIEnv *env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_ContentVideoView_onVideoSizeChanged(env, content_video_view.obj(),
        width, height);
  }
}

void ContentVideoView::OnBufferingUpdate(int percent) {
  JNIEnv *env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_ContentVideoView_onBufferingUpdate(env, content_video_view.obj(),
        percent);
  }
}

void ContentVideoView::OnPlaybackComplete() {
  JNIEnv *env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null())
    Java_ContentVideoView_onPlaybackComplete(env, content_video_view.obj());
}

void ContentVideoView::OnExitFullscreen() {
  DestroyContentVideoView(false);
}

void ContentVideoView::UpdateMediaMetadata() {
  JNIEnv *env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null())
    UpdateMediaMetadata(env, content_video_view.obj());
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
  manager_->FullscreenPlayerPlay();
}

void ContentVideoView::Pause(JNIEnv*, jobject obj) {
  manager_->FullscreenPlayerPause();
}

void ContentVideoView::ExitFullscreen(
    JNIEnv*, jobject, jboolean release_media_player) {
  j_content_video_view_.reset();
  manager_->ExitFullscreen(release_media_player);
}

void ContentVideoView::SetSurface(JNIEnv* env, jobject obj,
                                  jobject surface) {
  manager_->SetVideoSurface(
      gfx::ScopedJavaSurface::AcquireExternalSurface(surface));
}

void ContentVideoView::UpdateMediaMetadata(JNIEnv* env, jobject obj) {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  if (player && player->IsPlayerReady())
    Java_ContentVideoView_onUpdateMediaMetadata(
        env, obj, player->GetVideoWidth(), player->GetVideoHeight(),
        player->GetDuration().InMilliseconds(), player->CanPause(),
        player->CanSeekForward(), player->CanSeekBackward());
}

ScopedJavaLocalRef<jobject> ContentVideoView::GetJavaObject(JNIEnv* env) {
  return j_content_video_view_.get(env);
}

void ContentVideoView::DestroyContentVideoView(bool native_view_destroyed) {
  JNIEnv *env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_ContentVideoView_destroyContentVideoView(env,
        content_video_view.obj(), native_view_destroyed);
    j_content_video_view_.reset();
  }
}
}  // namespace content
