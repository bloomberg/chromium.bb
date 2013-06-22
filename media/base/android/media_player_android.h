// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/callback.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/media_export.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

class MediaDrmBridge;
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

  // Types of media source that this object will play.
  enum SourceType {
    SOURCE_TYPE_URL,
    SOURCE_TYPE_MSE,     // W3C Media Source Extensions
    SOURCE_TYPE_STREAM,  // W3C Media Stream, e.g. getUserMedia().
  };

  // Construct a MediaPlayerAndroid object with all the needed media player
  // callbacks. This object needs to call |manager_|'s RequestMediaResources()
  // before decoding the media stream. This allows |manager_| to track
  // unused resources and free them when needed. On the other hand, it needs
  // to call ReleaseMediaResources() when it is done with decoding.
  static MediaPlayerAndroid* Create(int player_id,
                                    const GURL& url,
                                    SourceType source_type,
                                    const GURL& first_party_for_cookies,
                                    bool hide_url_log,
                                    MediaPlayerManager* manager);

  // Passing an external java surface object to the player.
  virtual void SetVideoSurface(gfx::ScopedJavaSurface surface) = 0;

  // Start playing the media.
  virtual void Start() = 0;

  // Pause the media.
  virtual void Pause() = 0;

  // Seek to a particular position. When succeeds, OnSeekComplete() will be
  // called. Otherwise, nothing will happen.
  virtual void SeekTo(base::TimeDelta time) = 0;

  // Release the player resources.
  virtual void Release() = 0;

  // Set the player volume.
  virtual void SetVolume(float leftVolume, float rightVolume) = 0;

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

  // Methods for DemuxerStreamPlayer.
  // Informs DemuxerStreamPlayer that the demuxer is ready.
  virtual void DemuxerReady(
      const MediaPlayerHostMsg_DemuxerReady_Params& params);
  // Called when the requested data is received from the demuxer.
  virtual void ReadFromDemuxerAck(
      const MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params);

  // Called when a seek request is acked by the render process.
  virtual void OnSeekRequestAck(unsigned seek_request_id);

  // Called when the demuxer has changed the duration.
  virtual void DurationChanged(const base::TimeDelta& duration);

  // Pass a drm bridge to a player.
  virtual void SetDrmBridge(MediaDrmBridge* drm_bridge);

  int player_id() { return player_id_; }

 protected:
  MediaPlayerAndroid(int player_id,
                     MediaPlayerManager* manager);

  // Called when player status changes.
  virtual void OnMediaError(int error_type);
  virtual void OnVideoSizeChanged(int width, int height);
  virtual void OnBufferingUpdate(int percent);
  virtual void OnPlaybackComplete();
  virtual void OnSeekComplete();
  virtual void OnMediaMetadataChanged(
      base::TimeDelta duration, int width, int height, bool success);
  virtual void OnMediaInterrupted();
  virtual void OnTimeUpdated();

  // Request or release decoding resources from |manager_|.
  virtual void RequestMediaResourcesFromManager();
  virtual void ReleaseMediaResourcesFromManager();

  MediaPlayerManager* manager() { return manager_; }

 private:
  // Player ID assigned to this player.
  int player_id_;

  // Resource manager for all the media players.
  MediaPlayerManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerAndroid);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_
