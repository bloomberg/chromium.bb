// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_LISTENER_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_LISTENER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MediaPlayerAndroid;

// Helper class for posting MediaPlayerAndroid tasks on the UI thread.
class MediaPlayerListenerProxy
    : public base::RefCountedThreadSafe<MediaPlayerListenerProxy> {
 public:
  MediaPlayerListenerProxy(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::WeakPtr<MediaPlayerAndroid> media_player);
  void OnMediaError(int error_type);
  void OnVideoSizeChanged(int width, int height);
  void OnBufferingUpdate(int percent);
  void OnPlaybackComplete();
  void OnSeekComplete();
  void OnMediaPrepared();
  void OnMediaInterrupted();

 private:
  friend class base::RefCountedThreadSafe<MediaPlayerListenerProxy>;
  ~MediaPlayerListenerProxy();

  // The message loop where |media_player_| lives.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The MediaPlayerAndroid object all the callbacks should be sent to.
  base::WeakPtr<MediaPlayerAndroid> media_player_;
};

// Acts as a thread proxy between java MediaPlayerListener object and
// MediaPlayerAndroid so that callbacks are posted onto the UI thread.
class MediaPlayerListener {
 public:
  // Construct a native MediaPlayerListener object. Callbacks from the java
  // side object will be forwarded to |media_player| by posting a task on the
  // |task_runner|.
  MediaPlayerListener(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::WeakPtr<MediaPlayerAndroid> media_player);
  virtual ~MediaPlayerListener();

  // Called by the Java MediaPlayerListener and mirrored to corresponding
  // callbacks.
  void OnMediaError(JNIEnv* /* env */, jobject /* obj */, jint error_type);
  void OnVideoSizeChanged(JNIEnv* /* env */, jobject /* obj */,
                          jint width, jint height);
  void OnBufferingUpdate(JNIEnv* /* env */, jobject /* obj */, jint percent);
  void OnPlaybackComplete(JNIEnv* /* env */, jobject /* obj */);
  void OnSeekComplete(JNIEnv* /* env */, jobject /* obj */);
  void OnMediaPrepared(JNIEnv* /* env */, jobject /* obj */);
  void OnMediaInterrupted(JNIEnv* /* env */, jobject /* obj */);

  // Create a Java MediaPlayerListener object and listens to all the media
  // related events from system and |media_player|. If |media_player| is NULL,
  // this class only listens to system events.
  void CreateMediaPlayerListener(jobject context, jobject media_player);
  void ReleaseMediaPlayerListenerResources();

  // Register MediaPlayerListener in the system library loader.
  static bool RegisterMediaPlayerListener(JNIEnv* env);

 private:
  // Proxy for posting tasks to the UI thread.
  scoped_refptr<MediaPlayerListenerProxy> proxy_;

  base::android::ScopedJavaGlobalRef<jobject> j_media_player_listener_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerListener);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_LISTENER_H_
