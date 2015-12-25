// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

class BrowserMediaPlayerManager;

// Native mirror of ContentVideoView.java. This class is responsible for
// creating the Java video view and pass all the player status change to
// it. It accepts media control from Java class, and forwards it to
// MediaPlayerManagerImpl.
class ContentVideoView {
 public:
  // Construct a ContentVideoView object. The |manager| will handle all the
  // playback controls from the Java class.
  explicit ContentVideoView(BrowserMediaPlayerManager* manager);

  ~ContentVideoView();

  // To open another video on existing ContentVideoView.
  void OpenVideo();

  static bool RegisterContentVideoView(JNIEnv* env);
  static void KeepScreenOn(bool screen_on);

  // Return the singleton object or NULL.
  static ContentVideoView* GetInstance();

  // Getter method called by the Java class to get the media information.
  bool IsPlaying(JNIEnv*, const base::android::JavaParamRef<jobject>& obj);
  void RequestMediaMetadata(JNIEnv*,
                            const base::android::JavaParamRef<jobject>& obj);

  // Called when the Java fullscreen view is destroyed. If
  // |release_media_player| is true, |manager_| needs to release the player
  // as we are quitting the app.
  void ExitFullscreen(JNIEnv*,
                      const base::android::JavaParamRef<jobject>&,
                      jboolean release_media_player);

  // Called by the Java class to pass the surface object to the player.
  void SetSurface(JNIEnv*,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& surface);

  // Method called by |manager_| to inform the Java class about player status
  // change.
  void UpdateMediaMetadata();
  void OnMediaPlayerError(int errorType);
  void OnVideoSizeChanged(int width, int height);
  void OnPlaybackComplete();
  void OnExitFullscreen();

  // Functions called to record fullscreen playback UMA metrics.
  void RecordFullscreenPlayback(JNIEnv*,
                                const base::android::JavaParamRef<jobject>&,
                                bool is_portrait_video,
                                bool is_orientation_portrait);
  void RecordExitFullscreenPlayback(
      JNIEnv*,
      const base::android::JavaParamRef<jobject>&,
      bool is_portrait_video,
      long playback_duration_in_milliseconds_before_orientation_change,
      long playback_duration_in_milliseconds_after_orientation_change);

  // Return the corresponing ContentVideoView Java object if any.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env);

 private:
  // Creates the corresponding ContentVideoView Java object.
  JavaObjectWeakGlobalRef CreateJavaObject();

  // Object that manages the fullscreen media player. It is responsible for
  // handling all the playback controls.
  BrowserMediaPlayerManager* manager_;

  // Weak reference of corresponding Java object.
  JavaObjectWeakGlobalRef j_content_video_view_;

  // Weak pointer for posting tasks.
  base::WeakPtrFactory<ContentVideoView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentVideoView);
};

} // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_
