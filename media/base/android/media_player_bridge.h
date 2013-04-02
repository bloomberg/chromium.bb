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
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"
#include "media/base/media_export.h"
#include "media/base/android/media_player_listener.h"

namespace media {

class MediaResourceGetter;
class MediaPlayerBridgeManager;

// This class serves as a bridge for native code to call java functions inside
// android mediaplayer class. For more information on android mediaplayer, check
// http://developer.android.com/reference/android/media/MediaPlayer.html
// The actual android mediaplayer instance is created lazily when Start(),
// Pause(), SeekTo() gets called. As a result, media information may not
// be available until one of those operations is performed. After that, we
// will cache those information in case the mediaplayer gets released.
class MEDIA_EXPORT MediaPlayerBridge {
 public:
  // Error types for MediaErrorCB.
  enum MediaErrorType {
    MEDIA_ERROR_FORMAT,
    MEDIA_ERROR_DECODE,
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK,
    MEDIA_ERROR_INVALID_CODE,
  };

  // Callback when error happens. Args: player ID, error type.
  typedef base::Callback<void(int, int)> MediaErrorCB;

  // Callback when video size has changed. Args: player ID, width, height.
  typedef base::Callback<void(int, int, int)> VideoSizeChangedCB;

  // Callback when buffering has changed. Args: player ID, percentage
  // of the media.
  typedef base::Callback<void(int, int)> BufferingUpdateCB;

  // Callback when player got prepared. Args: player ID, duration of the media.
  typedef base::Callback<void(int, base::TimeDelta, int, int, bool)>
      MediaMetadataChangedCB;

  // Callbacks when seek completed. Args: player ID, current time.
  typedef base::Callback<void(int, base::TimeDelta)> SeekCompleteCB;

  // Callbacks when seek completed. Args: player ID
  typedef base::Callback<void(int)> MediaInterruptedCB;

  // Callbacks when playback completed. Args: player ID.
  typedef base::Callback<void(int)> PlaybackCompleteCB;

  // Callback when time update messages need to be sent. Args: player ID,
  // current time.
  typedef base::Callback<void(int, base::TimeDelta)> TimeUpdateCB;

  static bool RegisterMediaPlayerBridge(JNIEnv* env);

  // Construct a MediaPlayerBridge object with all the needed media player
  // callbacks. This object needs to call |manager|'s RequestMediaResources()
  // before decoding the media stream. This allows |manager| to track
  // unused resources and free them when needed. On the other hand, it needs
  // to call ReleaseMediaResources() when it is done with decoding.
  MediaPlayerBridge(int player_id,
                    const GURL& url,
                    const GURL& first_party_for_cookies,
                    MediaResourceGetter* resource_getter,
                    bool hide_url_log,
                    MediaPlayerBridgeManager* manager,
                    const MediaErrorCB& media_error_cb,
                    const VideoSizeChangedCB& video_size_changed_cb,
                    const BufferingUpdateCB& buffering_update_cb,
                    const MediaMetadataChangedCB& media_prepared_cb,
                    const PlaybackCompleteCB& playback_complete_cb,
                    const SeekCompleteCB& seek_complete_cb,
                    const TimeUpdateCB& time_update_cb,
                    const MediaInterruptedCB& media_interrupted_cb);
  ~MediaPlayerBridge();

  typedef std::map<std::string, std::string> HeadersMap;

  void SetVideoSurface(jobject surface);

  // Start playing the media.
  void Start();

  // Pause the media.
  void Pause();

  // Seek to a particular position. When succeeds, OnSeekComplete() will be
  // called. Otherwise, nothing will happen.
  void SeekTo(base::TimeDelta time);

  // Release the player resources.
  void Release();

  // Set the player volume.
  void SetVolume(float leftVolume, float rightVolume);

  // Get the media information from the player.
  int GetVideoWidth();
  int GetVideoHeight();
  base::TimeDelta GetCurrentTime();
  base::TimeDelta GetDuration();
  bool IsPlaying();

  // Get allowed operations from the player.
  void GetAllowedOperations();

  // Called by the timer to check for current time routinely and generates
  // time update events.
  void DoTimeUpdate();

  // Called by the MediaPlayerListener and mirrored to corresponding
  // callbacks.
  void OnMediaError(int error_type);
  void OnVideoSizeChanged(int width, int height);
  void OnBufferingUpdate(int percent);
  void OnPlaybackComplete();
  void OnSeekComplete();
  void OnMediaPrepared();
  void OnMediaInterrupted();

  // Prepare the player for playback, asynchronously. When succeeds,
  // OnMediaPrepared() will be called. Otherwise, OnMediaError() will
  // be called with an error type.
  void Prepare();

  // Callback function passed to |resource_getter_|. Called when the cookies
  // are retrieved.
  void OnCookiesRetrieved(const std::string& cookies);

  int player_id() { return player_id_; }
  bool can_pause() { return can_pause_; }
  bool can_seek_forward() { return can_seek_forward_; }
  bool can_seek_backward() { return can_seek_backward_; }
  bool prepared() { return prepared_; }

 private:
  // Initialize this object and extract the metadata from the media.
  void Initialize();

  // Create the actual android media player.
  void CreateMediaPlayer();

  // Set the data source for the media player.
  void SetDataSource(const std::string& url);

  // Functions that implements media player control.
  void StartInternal();
  void PauseInternal();
  void SeekInternal(base::TimeDelta time);

  // Extract the media metadata from a url, asynchronously.
  // OnMediaMetadataExtracted() will be called when this call finishes.
  void ExtractMediaMetadata(const std::string& url);
  void OnMediaMetadataExtracted(base::TimeDelta duration, int width, int height,
                                bool success);

  // Callbacks when events are received.
  MediaErrorCB media_error_cb_;
  VideoSizeChangedCB video_size_changed_cb_;
  BufferingUpdateCB buffering_update_cb_;
  MediaMetadataChangedCB media_metadata_changed_cb_;
  PlaybackCompleteCB playback_complete_cb_;
  SeekCompleteCB seek_complete_cb_;
  MediaInterruptedCB media_interrupted_cb_;

  // Callbacks when timer events are received.
  TimeUpdateCB time_update_cb_;

  // Player ID assigned to this player.
  int player_id_;

  // Whether the player is prepared for playback.
  bool prepared_;

  // Pending play event while player is preparing.
  bool pending_play_;

  // Pending seek time while player is preparing.
  base::TimeDelta pending_seek_;

  // Url for playback.
  GURL url_;

  // First party url for cookies.
  GURL first_party_for_cookies_;

  // Hide url log from media player.
  bool hide_url_log_;

  // Stats about the media.
  base::TimeDelta duration_;
  int width_;
  int height_;

  // Meta data about actions can be taken.
  bool can_pause_;
  bool can_seek_forward_;
  bool can_seek_backward_;

  // Cookies for |url_|.
  std::string cookies_;

  // Resource manager for all the media players.
  MediaPlayerBridgeManager* manager_;

  // Object for retrieving resources for this media player.
  scoped_ptr<MediaResourceGetter> resource_getter_;

  // Java MediaPlayer instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_player_;

  base::RepeatingTimer<MediaPlayerBridge> time_update_timer_;

  // Weak pointer passed to |listener_| for callbacks.
  base::WeakPtrFactory<MediaPlayerBridge> weak_this_;

  // Listener object that listens to all the media player events.
  MediaPlayerListener listener_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_
