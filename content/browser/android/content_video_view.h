// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"

namespace webkit_media {
struct MediaMetadataAndroid;
}

namespace content {

class MediaPlayerDelegateAndroid;

// Native mirror of ContentVideoView.java.
class ContentVideoView {
 public:
  ContentVideoView();
  ~ContentVideoView();

  static bool RegisterContentVideoView(JNIEnv* env);
  void Init(JNIEnv*, jobject obj, jobject weak_this);

  // --------------------------------------------------------------------------
  // All these functions are called on UI thread
  int GetVideoWidth(JNIEnv*, jobject obj) const;
  int GetVideoHeight(JNIEnv*, jobject obj) const;
  int GetDurationInMilliSeconds(JNIEnv*, jobject obj) const;
  int GetCurrentPosition(JNIEnv*, jobject obj) const;
  bool IsPlaying(JNIEnv*, jobject obj);
  void SeekTo(JNIEnv*, jobject obj, jint msec);
  int GetPlayerId(JNIEnv*, jobject obj) const;
  int GetRouteId(JNIEnv*, jobject obj) const;
  int GetRenderHandle(JNIEnv*, jobject obj) const;
  void Play(JNIEnv*, jobject obj);
  void Pause(JNIEnv*, jobject obj);
  // --------------------------------------------------------------------------

  void PrepareAsync();
  void DestroyContentVideoView();
  void UpdateMediaMetadata(JNIEnv*, jobject obj);
  void DestroyContentVideoView(JNIEnv*, jobject);
  void DestroyContentVideoView(JNIEnv*, jobject, jboolean release_media_player);
  void CreateContentVideoView(MediaPlayerDelegateAndroid* player);
  void SetSurface(JNIEnv*,
                  jobject obj,
                  jobject surface,
                  jint route_id,
                  jint player_id);
  void UpdateMediaMetadata();

  void OnMediaPlayerError(int errorType);
  void OnVideoSizeChanged(int width, int height);
  void OnBufferingUpdate(int percent);
  void OnPlaybackComplete();

 private:
  // In some certain cases if the renderer crashes, the ExitFullscreen message
  // will never acknowledged by the renderer.
  void OnTimeout();

  webkit_media::MediaMetadataAndroid* GetMediaMetadata();

  int ConvertSecondsToMilliSeconds(float seconds) const;

  MediaPlayerDelegateAndroid* player_;

  base::android::ScopedJavaGlobalRef<jobject> j_content_video_view_;

  // A timer to keep track of when the acknowledgement of exitfullscreen
  // message times out.
  base::OneShotTimer<ContentVideoView> timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ContentVideoView);
};

} // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_
