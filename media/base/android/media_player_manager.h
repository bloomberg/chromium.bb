// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_

#include "base/time.h"
#include "media/base/media_export.h"
#if defined(GOOGLE_TV)
#include "media/base/android/demuxer_stream_player_params.h"
#endif

namespace media {

class MediaPlayerAndroid;
class MediaResourceGetter;

// This class is responsible for managing active MediaPlayerAndroid objects.
// It is implemented by content::MediaPlayerManagerImpl.
class MEDIA_EXPORT MediaPlayerManager {
 public:
  virtual ~MediaPlayerManager();

  // Called by a MediaPlayerAndroid object when it is going to decode
  // media streams. This helps the manager object maintain an array
  // of active MediaPlayerAndroid objects and release the resources
  // when needed.
  virtual void RequestMediaResources(MediaPlayerAndroid* player) = 0;

  // Called when a MediaPlayerAndroid object releases all its decoding
  // resources.
  virtual void ReleaseMediaResources(MediaPlayerAndroid* player) = 0;

  // Return a pointer to the MediaResourceGetter object.
  virtual MediaResourceGetter* GetMediaResourceGetter() = 0;

  // Called when time update messages need to be sent. Args: player ID,
  // current time.
  virtual void OnTimeUpdate(int player_id, base::TimeDelta current_time) = 0;

  // Called when media metadata changed. Args: player ID, duration of the
  // media, width, height, whether the metadata is successfully extracted.
  virtual void OnMediaMetadataChanged(
      int player_id,
      base::TimeDelta duration,
      int width,
      int height,
      bool success) = 0;

  // Called when playback completed. Args: player ID.
  virtual void OnPlaybackComplete(int player_id) = 0;

  // Called when media download was interrupted. Args: player ID.
  virtual void OnMediaInterrupted(int player_id) = 0;

  // Called when buffering has changed. Args: player ID, percentage
  // of the media.
  virtual void OnBufferingUpdate(int player_id, int percentage) = 0;

  // Called when seek completed. Args: player ID, current time.
  virtual void OnSeekComplete(int player_id, base::TimeDelta current_time) = 0;

  // Called when error happens. Args: player ID, error type.
  virtual void OnError(int player_id, int error) = 0;

  // Called when video size has changed. Args: player ID, width, height.
  virtual void OnVideoSizeChanged(int player_id, int width, int height) = 0;

#if defined(GOOGLE_TV)
  // Callback when DemuxerStreamPlayer wants to read data from the demuxer.
  virtual void OnReadFromDemuxer(
      int player_id, media::DemuxerStream::Type type, bool seek_done) = 0;
#endif
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
