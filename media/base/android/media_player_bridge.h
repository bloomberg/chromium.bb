// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_

#include <jni.h>
#include <map>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/time.h"

namespace media {

// This class serves as a bridge for native code to call java functions inside
// android mediaplayer class. For more information on android mediaplayer, check
// http://developer.android.com/reference/android/media/MediaPlayer.html
// To use this class, follow the state diagram listed in the above url.
// Here is the normal work flow for this class:
// 1. Call SetDataSource() to set the media url.
// 2. Call Prepare() to prepare the player for playback. This is a non
//    blocking call.
// 3. When Prepare() succeeds, OnMediaPrepared() will get called.
// 4. Call Start(), Pause(), SeekTo() to play/pause/seek the media.
class MediaPlayerBridge {
 public:
  // Error types for MediaErrorCB.
  enum MediaErrorType {
    MEDIA_ERROR_UNKNOWN,
    MEDIA_ERROR_SERVER_DIED,
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK,
    MEDIA_ERROR_INVALID_CODE,
  };

  // Info types for MediaInfoCB.
  enum MediaInfoType {
    MEDIA_INFO_UNKNOWN,
    MEDIA_INFO_VIDEO_TRACK_LAGGING,
    MEDIA_INFO_BUFFERING_START,
    MEDIA_INFO_BUFFERING_END,
    MEDIA_INFO_BAD_INTERLEAVING,
    MEDIA_INFO_NOT_SEEKABLE,
    MEDIA_INFO_METADATA_UPDATE,
  };

  // Callback when video info is received. Args: info type.
  typedef base::Callback<void(int)> MediaInfoCB;

  // Callback when error happens. Args: error type.
  typedef base::Callback<void(int)> MediaErrorCB;

  // Callback when video size has changed. Args: width, height.
  typedef base::Callback<void(int,int)> VideoSizeChangedCB;

  // Callback when buffering has changed. Args: percentage of the media.
  typedef base::Callback<void(int)> BufferingUpdateCB;

  MediaPlayerBridge();
  ~MediaPlayerBridge();

  typedef std::map<std::string, std::string> HeadersMap;
  void SetDataSource(const std::string& url,
                     const std::string& cookies,
                     bool hide_url_log);

  void SetVideoSurface(jobject surface);

  // Prepare the player for playback, asynchronously. When succeeds,
  // OnMediaPrepared() will be called. Otherwise, OnMediaError() will
  // be called with an error type.
  void Prepare(const MediaInfoCB& media_info_cb,
               const MediaErrorCB& media_error_cb,
               const VideoSizeChangedCB& video_size_changed_cb,
               const BufferingUpdateCB& buffering_update_cb,
               const base::Closure& media_prepared_cb);

  // Start playing the media.
  void Start(const base::Closure& playback_complete_cb);

  // Pause the media.
  void Pause();

  // Stop the media playback. Needs to call Prepare() again to play the media.
  void Stop();

  // Seek to a particular position. When succeeds, OnSeekComplete() will be
  // called. Otherwise, nothing will happen.
  void SeekTo(base::TimeDelta time, const base::Closure& seek_complete_cb);

  // Reset the player. Needs to call SetDataSource() again after this call.
  void Reset();

  // Set the player volume.
  void SetVolume(float leftVolume, float rightVolume);

  // Get the media information from the player.
  int GetVideoWidth();
  int GetVideoHeight();
  base::TimeDelta GetCurrentTime();
  base::TimeDelta GetDuration();
  bool IsPlaying();

  // Get metadata from the media.
  void GetMetadata(bool* can_pause,
                   bool* can_seek_forward,
                   bool* can_seek_backward);

  // Set the device to stay awake when player is playing.
  void SetStayAwakeWhilePlaying();

  // Called by the Java MediaPlayerListener and mirrored to corresponding
  // callbacks.
  void OnMediaError(JNIEnv* /* env */, jobject /* obj */, jint error_type);
  void OnMediaInfo(JNIEnv* /* env */, jobject /* obj */, jint info_type);
  void OnVideoSizeChanged(JNIEnv* /* env */, jobject /* obj */,
                          jint width, jint height);
  void OnBufferingUpdate(JNIEnv* /* env */, jobject /* obj */, jint percent);
  void OnPlaybackComplete(JNIEnv* /* env */, jobject /* obj */);
  void OnSeekComplete(JNIEnv* /* env */, jobject /* obj */);
  void OnMediaPrepared(JNIEnv* /* env */, jobject /* obj */);

  // Register MediaPlayerListener in the system library loader.
  static bool RegisterMediaPlayerListener(JNIEnv* env);

 private:
  void CallVoidMethod(std::string method_name);
  int CallIntMethod(std::string method_name);

  // Callbacks when events are received.
  MediaInfoCB media_info_cb_;
  MediaErrorCB media_error_cb_;
  VideoSizeChangedCB video_size_changed_cb_;
  BufferingUpdateCB buffering_update_cb_;
  base::Closure playback_complete_cb_;
  base::Closure seek_complete_cb_;
  base::Closure media_prepared_cb_;

  // Java MediaPlayer class and instance.
  base::android::ScopedJavaGlobalRef<jclass> j_media_player_class_;
  base::android::ScopedJavaGlobalRef<jobject> j_media_player_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_
