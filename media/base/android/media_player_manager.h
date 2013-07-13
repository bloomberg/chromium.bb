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

namespace content {
class RenderViewHost;
}

namespace media {

class MediaDrmBridge;
class MediaPlayerAndroid;
class MediaResourceGetter;

// This class is responsible for managing active MediaPlayerAndroid objects.
// Objects implementing this interface a created via
// MediaPlayerManager::Create(), allowing embedders to provide their
// implementation.
class MEDIA_EXPORT MediaPlayerManager {
 public:
  // The type of the factory function that returns a new instance of the
  // MediaPlayerManager implementation.
  typedef MediaPlayerManager* (*FactoryFunction)(content::RenderViewHost*);

  // Allows to override the default factory function in order to provide
  // a custom implementation to the RenderViewHost instance.
  // Must be called from the main thread.
  static void RegisterFactoryFunction(FactoryFunction factory_function);

  // Returns a new instance of MediaPlayerManager interface implementation.
  // The returned object is owned by the caller. Must be called on the main
  // thread.
  static MediaPlayerManager* Create(content::RenderViewHost* render_view_host);

  virtual ~MediaPlayerManager() {}

  // Called by a MediaPlayerAndroid object when it is going to decode
  // media streams. This helps the manager object maintain an array
  // of active MediaPlayerAndroid objects and release the resources
  // when needed.
  virtual void RequestMediaResources(int player_id) = 0;

  // Called when a MediaPlayerAndroid object releases all its decoding
  // resources.
  virtual void ReleaseMediaResources(int player_id) = 0;

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

  // Returns the player that's in the fullscreen mode currently.
  virtual MediaPlayerAndroid* GetFullscreenPlayer() = 0;

  // Returns the player with the specified id.
  virtual MediaPlayerAndroid* GetPlayer(int player_id) = 0;

  // Release all the players managed by this object.
  virtual void DestroyAllMediaPlayers() = 0;

  // Callback when DemuxerStreamPlayer wants to read data from the demuxer.
  virtual void OnReadFromDemuxer(int player_id,
                                 media::DemuxerStream::Type type) = 0;

  // Called when player wants the media element to initiate a seek.
  virtual void OnMediaSeekRequest(int player_id, base::TimeDelta time_to_seek,
                                  unsigned seek_request_id) = 0;

  // Called when player wants to read the config data from the demuxer.
  virtual void OnMediaConfigRequest(int player_id) = 0;

  // Get the MediaDrmBridge object for the given media key Id.
  virtual media::MediaDrmBridge* GetDrmBridge(int media_keys_id) = 0;

  // Called by the player to get a hardware protected surface.
  virtual void OnProtectedSurfaceRequested(int player_id) = 0;

  // TODO(xhwang): The following three methods needs to be decoupled from
  // MediaPlayerManager to support the W3C Working Draft version of the EME
  // spec.

  // Called when MediaDrmBridge wants to send a KeyAdded.
  virtual void OnKeyAdded(int media_keys_id,
                          const std::string& session_id) = 0;

  // Called when MediaDrmBridge wants to send a KeyError.
  virtual void OnKeyError(int media_keys_id,
                          const std::string& session_id,
                          media::MediaKeys::KeyError error_code,
                          int system_code) = 0;

  // Called when MediaDrmBridge wants to send a KeyMessage.
  virtual void OnKeyMessage(int media_keys_id,
                            const std::string& session_id,
                            const std::vector<uint8>& message,
                            const std::string& destination_url) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
