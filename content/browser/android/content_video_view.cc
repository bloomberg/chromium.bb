// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_video_view.h"

#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "base/logging.h"
#if !defined(ANDROID_UPSTREAM_BRINGUP)
#include "content/browser/media/media_player_delegate_android.h"
#endif
#include "content/common/android/surface_callback.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "jni/content_video_view_jni.h"
#include "webkit/media/android/media_metadata_android.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaGlobalRef;

// The timeout value we should wait after user click the back button on the
// fullscreen view.
static const int kTimeoutMillseconds = 1000;

// ----------------------------------------------------------------------------
// Methods that call to Java via JNI
// ----------------------------------------------------------------------------

namespace content {

// static
bool ContentVideoView::RegisterContentVideoView(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ContentVideoView::CreateContentVideoView(
    MediaPlayerDelegateAndroid* player) {
  player_ = player;
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

// ----------------------------------------------------------------------------

ContentVideoView::ContentVideoView() : player_(NULL) {
}

ContentVideoView::~ContentVideoView() {
  player_ = NULL;

  // If the browser process crashed, just kill the fullscreen view.
  DestroyContentVideoView();
}

// ----------------------------------------------------------------------------
// All these functions are called on UI thread

int ContentVideoView::GetVideoWidth(JNIEnv*, jobject obj) const {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return player_ ? player_->GetVideoWidth() : 0;
#else
  return 0;
#endif
}

int ContentVideoView::GetVideoHeight(JNIEnv*, jobject obj) const {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return player_ ? player_->GetVideoHeight() : 0;
#else
  return 0;
#endif
}

int ContentVideoView::GetDurationInMilliSeconds(JNIEnv*, jobject obj) const {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return player_ ? ConvertSecondsToMilliSeconds(player_->Duration()) : -1;
#else
  return -1;
#endif
}

int ContentVideoView::GetCurrentPosition(JNIEnv*, jobject obj) const {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return player_ ? ConvertSecondsToMilliSeconds(player_->CurrentTime()) : 0;
#else
  return 0;
#endif
}

bool ContentVideoView::IsPlaying(JNIEnv*, jobject obj) {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return player_ ? player_->IsPlaying() : false;
#else
  return false;
#endif
}

void ContentVideoView::SeekTo(JNIEnv*, jobject obj, jint msec) {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (player_)
    player_->Seek(static_cast<float>(msec / 1000.0));
#endif
}

webkit_media::MediaMetadataAndroid* ContentVideoView::GetMediaMetadata() {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!player_)
    return NULL;

  return new webkit_media::MediaMetadataAndroid(player_->GetVideoWidth(),
                                                player_->GetVideoHeight(),
                                                base::TimeDelta::FromSeconds(
                                                    player_->Duration()),
                                                base::TimeDelta::FromSeconds(
                                                    player_->CurrentTime()),
                                                !player_->IsPlaying(),
                                                player_->CanPause(),
                                                player_->CanSeekForward(),
                                                player_->CanSeekForward());
#else
  return NULL;
#endif
}

int ContentVideoView::GetPlayerId(JNIEnv*, jobject obj) const {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return player_ ? player_->player_id() : -1;
#else
  return -1;
#endif
}

int ContentVideoView::GetRouteId(JNIEnv*, jobject obj) const {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return player_ ? player_->route_id() : -1;
#else
  return -1;
#endif
}

int ContentVideoView::GetRenderHandle(JNIEnv*, jobject obj) const {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static bool single_process =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
  if (!player_)
    return -1;

  if (single_process)
    return 0;
  return player_->render_handle();
#else
  return -1;
#endif
}

void ContentVideoView::Play(JNIEnv*, jobject obj) {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (player_)
    player_->Play();
#endif
}

void ContentVideoView::Pause(JNIEnv*, jobject obj) {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (player_)
    player_->Pause();
#endif
}

void ContentVideoView::OnTimeout() {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#endif
  timeout_timer_.Stop();
  DestroyContentVideoView();
}

int ContentVideoView::ConvertSecondsToMilliSeconds(float seconds) const {
  return static_cast<int>(seconds * 1000);
}

// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

void ContentVideoView::DestroyContentVideoView(JNIEnv*, jobject,
                                               jboolean release_media_player) {
#if !defined(ANDROID_UPSTREAM_BRINGUP)
  if (player_) {
    player_->ExitFullscreen(release_media_player);

    // Fire off a timer so that we will close the fullscreen view in case the
    // renderer crashes.
    timeout_timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(kTimeoutMillseconds),
                   this, &ContentVideoView::OnTimeout);
  }
#endif
}

void ContentVideoView::SetSurface(JNIEnv* env, jobject obj,
                                  jobject surface,
                                  jint route_id,
                                  jint player_id) {
  SetSurfaceAsync(env,
                  surface,
                  SurfaceTexturePeer::SET_VIDEO_SURFACE_TEXTURE,
                  route_id,
                  player_id,
                  NULL);
}

void ContentVideoView::UpdateMediaMetadata(JNIEnv* env, jobject obj) {
  scoped_ptr<webkit_media::MediaMetadataAndroid> metadata(GetMediaMetadata());
  Java_ContentVideoView_updateMediaMetadata(env,
                                            obj,
                                            metadata->width,
                                            metadata->height,
                                            metadata->duration.InMilliseconds(),
                                            metadata->can_pause,
                                            metadata->can_seek_forward,
                                            metadata->can_seek_backward);
}

} // namespace content
