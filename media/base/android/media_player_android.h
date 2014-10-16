// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/android/media_player_listener.h"
#include "media/base/media_export.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "url/gurl.h"

namespace media {

class BrowserCdm;
class MediaPlayerManager;

// This class serves as the base class for different media player
// implementations on Android. Subclasses need to provide their own
// MediaPlayerAndroid::Create() implementation.
class MEDIA_EXPORT MediaPlayerAndroid {
 public:
  virtual ~MediaPlayerAndroid();

  // Error types for MediaErrorCB.
  enum MediaErrorType {
    MEDIA_ERROR_FORMAT,
    MEDIA_ERROR_DECODE,
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK,
    MEDIA_ERROR_INVALID_CODE,
  };

  // Callback when the player needs decoding resources.
  typedef base::Callback<void(int player_id)> RequestMediaResourcesCB;

  // Passing an external java surface object to the player.
  virtual void SetVideoSurface(gfx::ScopedJavaSurface surface) = 0;

  // Start playing the media.
  virtual void Start() = 0;

  // Pause the media.
  virtual void Pause(bool is_media_related_action) = 0;

  // Seek to a particular position, based on renderer signaling actual seek
  // with MediaPlayerHostMsg_Seek. If eventual success, OnSeekComplete() will be
  // called.
  virtual void SeekTo(base::TimeDelta timestamp) = 0;

  // Release the player resources.
  virtual void Release() = 0;

  // Set the player volume.
  virtual void SetVolume(double volume) = 0;

  // Get the media information from the player.
  virtual int GetVideoWidth() = 0;
  virtual int GetVideoHeight() = 0;
  virtual base::TimeDelta GetDuration() = 0;
  virtual base::TimeDelta GetCurrentTime() = 0;
  virtual bool IsPlaying() = 0;
  virtual bool IsPlayerReady() = 0;
  virtual bool CanPause() = 0;
  virtual bool CanSeekForward() = 0;
  virtual bool CanSeekBackward() = 0;
  virtual GURL GetUrl();
  virtual GURL GetFirstPartyForCookies();

  // Associates the |cdm| with this player.
  virtual void SetCdm(BrowserCdm* cdm);

  int player_id() { return player_id_; }

  GURL frame_url() { return frame_url_; }

 protected:
  MediaPlayerAndroid(int player_id,
                     MediaPlayerManager* manager,
                     const RequestMediaResourcesCB& request_media_resources_cb,
                     const GURL& frame_url);

  // TODO(qinmin): Simplify the MediaPlayerListener class to only listen to
  // media interrupt events. And have a separate child class to listen to all
  // the events needed by MediaPlayerBridge. http://crbug.com/422597.
  // MediaPlayerListener callbacks.
  virtual void OnVideoSizeChanged(int width, int height);
  virtual void OnMediaError(int error_type);
  virtual void OnBufferingUpdate(int percent);
  virtual void OnPlaybackComplete();
  virtual void OnMediaInterrupted();
  virtual void OnSeekComplete();
  virtual void OnMediaPrepared();

  // Attach/Detaches |listener_| for listening to all the media events. If
  // |j_media_player| is NULL, |listener_| only listens to the system media
  // events. Otherwise, it also listens to the events from |j_media_player|.
  void AttachListener(jobject j_media_player);
  void DetachListener();

  MediaPlayerManager* manager() { return manager_; }

  RequestMediaResourcesCB request_media_resources_cb_;

 private:
  friend class MediaPlayerListener;

  // Player ID assigned to this player.
  int player_id_;

  // Resource manager for all the media players.
  MediaPlayerManager* manager_;

  // Url for the frame that contains this player.
  GURL frame_url_;

  // Listener object that listens to all the media player events.
  scoped_ptr<MediaPlayerListener> listener_;

  // Weak pointer passed to |listener_| for callbacks.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaPlayerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerAndroid);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_
