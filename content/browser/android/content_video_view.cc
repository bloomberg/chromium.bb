// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_video_view.h"

#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/android/media_player_manager_android.h"
#include "content/common/android/surface_callback.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/public/common/content_switches.h"
#include "jni/ContentVideoView_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaGlobalRef;

namespace content {

bool ContentVideoView::RegisterContentVideoView(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ContentVideoView::ContentVideoView(MediaPlayerManagerAndroid* manager)
    : manager_(manager) {
}

ContentVideoView::~ContentVideoView() {
  DestroyContentVideoView();
}

void ContentVideoView::CreateContentVideoView() {
  if (j_content_video_view_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    j_content_video_view_.Reset(Java_ContentVideoView_createContentVideoView(
        env, reinterpret_cast<jint>(this)));
  } else {
    // Just ask video view to reopen the video.
    Java_ContentVideoView_openVideo(AttachCurrentThread(),
                                    j_content_video_view_.obj());
  }
}

void ContentVideoView::DestroyContentVideoView() {
  if (!j_content_video_view_.is_null()) {
    Java_ContentVideoView_destroyContentVideoView(AttachCurrentThread());
    j_content_video_view_.Reset();
  }
}

void ContentVideoView::OnMediaPlayerError(int error_type) {
  if (!j_content_video_view_.is_null()) {
    Java_ContentVideoView_onMediaPlayerError(AttachCurrentThread(),
                                             j_content_video_view_.obj(),
                                             error_type);
  }
}

void ContentVideoView::OnVideoSizeChanged(int width, int height) {
  if (!j_content_video_view_.is_null()) {
    Java_ContentVideoView_onVideoSizeChanged(AttachCurrentThread(),
                                             j_content_video_view_.obj(),
                                             width,
                                             height);
  }
}

void ContentVideoView::OnBufferingUpdate(int percent) {
  if (!j_content_video_view_.is_null()) {
    Java_ContentVideoView_onBufferingUpdate(AttachCurrentThread(),
                                            j_content_video_view_.obj(),
                                            percent);
  }
}

void ContentVideoView::OnPlaybackComplete() {
  if (!j_content_video_view_.is_null()) {
    Java_ContentVideoView_onPlaybackComplete(AttachCurrentThread(),
                                             j_content_video_view_.obj());
  }
}

void ContentVideoView::UpdateMediaMetadata() {
  if (!j_content_video_view_.is_null())
    UpdateMediaMetadata(AttachCurrentThread(), j_content_video_view_.obj());
}

int ContentVideoView::GetVideoWidth(JNIEnv*, jobject obj) const {
  media::MediaPlayerBridge* player = manager_->GetFullscreenPlayer();
  return player ? player->GetVideoWidth() : 0;
}

int ContentVideoView::GetVideoHeight(JNIEnv*, jobject obj) const {
  media::MediaPlayerBridge* player = manager_->GetFullscreenPlayer();
  return player ? player->GetVideoHeight() : 0;
}

int ContentVideoView::GetDurationInMilliSeconds(JNIEnv*, jobject obj) const {
  media::MediaPlayerBridge* player = manager_->GetFullscreenPlayer();
  return player ? player->GetDuration().InMilliseconds() : -1;
}

int ContentVideoView::GetCurrentPosition(JNIEnv*, jobject obj) const {
  media::MediaPlayerBridge* player = manager_->GetFullscreenPlayer();
  return player ? player->GetCurrentTime().InMilliseconds() : 0;
}

bool ContentVideoView::IsPlaying(JNIEnv*, jobject obj) {
  media::MediaPlayerBridge* player = manager_->GetFullscreenPlayer();
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
  manager_->ExitFullscreen(release_media_player);
  j_content_video_view_.Reset();
}

void ContentVideoView::SetSurface(JNIEnv* env, jobject obj,
                                  jobject surface) {
  manager_->SetVideoSurface(surface);
}

void ContentVideoView::UpdateMediaMetadata(JNIEnv* env, jobject obj) {
  media::MediaPlayerBridge* player = manager_->GetFullscreenPlayer();
  if (player && player->prepared())
    Java_ContentVideoView_updateMediaMetadata(
        env, obj, player->GetVideoWidth(), player->GetVideoHeight(),
        player->GetDuration().InMilliseconds(), player->can_pause(),
        player->can_seek_forward(), player->can_seek_backward());
}

}  // namespace content
