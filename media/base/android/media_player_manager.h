// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"

namespace media {

class MediaDrmBridge;
class MediaPlayerAndroid;
class MediaResourceGetter;

// This class is responsible for managing active MediaPlayerAndroid objects.
class MEDIA_EXPORT MediaPlayerManager {
 public:
  virtual ~MediaPlayerManager() {}

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
  virtual void OnSeekComplete(
      int player_id,
      const base::TimeDelta& current_time) = 0;

  // Called when error happens. Args: player ID, error type.
  virtual void OnError(int player_id, int error) = 0;

  // Called when video size has changed. Args: player ID, width, height.
  virtual void OnVideoSizeChanged(int player_id, int width, int height) = 0;

  // Returns the player that's in the fullscreen mode currently.
  virtual MediaPlayerAndroid* GetFullscreenPlayer() = 0;

  // Returns the player with the specified id.
  virtual MediaPlayerAndroid* GetPlayer(int player_id) = 0;

  // Release all the players managed by this object.
  virtual void DestroyAllMediaPlayers() = 0;

  // Get the MediaDrmBridge object for the given media key Id.
  virtual media::MediaDrmBridge* GetDrmBridge(int cdm_id) = 0;

  // Called by the player to get a hardware protected surface.
  virtual void OnProtectedSurfaceRequested(int player_id) = 0;

  // The following five methods are related to EME.
  // TODO(xhwang): These methods needs to be decoupled from MediaPlayerManager
  // to support the W3C Working Draft version of the EME spec.
  // http://crbug.com/315312

  // Called when MediaDrmBridge determines a SessionId.
  virtual void OnSessionCreated(int cdm_id,
                                uint32 session_id,
                                const std::string& web_session_id) = 0;

  // Called when MediaDrmBridge wants to send a Message event.
  virtual void OnSessionMessage(int cdm_id,
                                uint32 session_id,
                                const std::vector<uint8>& message,
                                const GURL& destination_url) = 0;

  // Called when MediaDrmBridge wants to send a Ready event.
  virtual void OnSessionReady(int cdm_id, uint32 session_id) = 0;

  // Called when MediaDrmBridge wants to send a Closed event.
  virtual void OnSessionClosed(int cdm_id, uint32 session_id) = 0;

  // Called when MediaDrmBridge wants to send an Error event.
  virtual void OnSessionError(int cdm_id,
                              uint32 session_id,
                              media::MediaKeys::KeyError error_code,
                              int system_code) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
