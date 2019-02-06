// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

class MediaPlayerAndroid;
class MediaResourceGetter;
class MediaUrlInterceptor;

// This class is responsible for managing active MediaPlayerAndroid objects.
class MEDIA_EXPORT MediaPlayerManager {
 public:
  virtual ~MediaPlayerManager() {}

  // Returns a pointer to the MediaResourceGetter object.
  virtual MediaResourceGetter* GetMediaResourceGetter() = 0;

  // Returns a pointer to the MediaUrlInterceptor object or null.
  virtual MediaUrlInterceptor* GetMediaUrlInterceptor() = 0;

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

  // Called when error happens. Args: player ID, error type.
  virtual void OnError(int player_id, int error) = 0;

  // Called when video size has changed. Args: player ID, width, height.
  virtual void OnVideoSizeChanged(int player_id, int width, int height) = 0;

  // Called by the player to request the playback for given duration. The
  // manager should use this opportunity to check if the current context is
  // appropriate for a media to play.
  // Returns whether the request was granted.
  virtual bool RequestPlay(int player_id,
                           base::TimeDelta duration,
                           bool has_audio) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
