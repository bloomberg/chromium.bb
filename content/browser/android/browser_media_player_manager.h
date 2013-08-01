// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_
#define CONTENT_BROWSER_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "content/browser/android/content_video_view.h"
#include "content/public/browser/render_view_host_observer.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/android/media_player_android.h"
#include "media/base/android/media_player_manager.h"
#include "ui/gfx/rect_f.h"
#include "url/gurl.h"

namespace media {
class MediaDrmBridge;
}

namespace content {

class WebContents;

// This class manages all the MediaPlayerAndroid objects. It receives
// control operations from the the render process, and forwards
// them to corresponding MediaPlayerAndroid object. Callbacks from
// MediaPlayerAndroid objects are converted to IPCs and then sent to the
// render process.
class CONTENT_EXPORT BrowserMediaPlayerManager
    : public RenderViewHostObserver,
      public media::MediaPlayerManager {
 public:
  virtual ~BrowserMediaPlayerManager();

  // RenderViewHostObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Fullscreen video playback controls.
  virtual void FullscreenPlayerPlay();
  virtual void FullscreenPlayerPause();
  virtual void FullscreenPlayerSeek(int msec);
  virtual void ExitFullscreen(bool release_media_player);
  virtual void SetVideoSurface(gfx::ScopedJavaSurface surface);

  // media::MediaPlayerManager overrides.
  virtual void OnTimeUpdate(
      int player_id, base::TimeDelta current_time) OVERRIDE;
  virtual void OnMediaMetadataChanged(
      int player_id,
      base::TimeDelta duration,
      int width,
      int height,
      bool success) OVERRIDE;
  virtual void OnPlaybackComplete(int player_id) OVERRIDE;
  virtual void OnMediaInterrupted(int player_id) OVERRIDE;
  virtual void OnBufferingUpdate(int player_id, int percentage) OVERRIDE;
  virtual void OnSeekComplete(
      int player_id, base::TimeDelta current_time) OVERRIDE;
  virtual void OnError(int player_id, int error) OVERRIDE;
  virtual void OnVideoSizeChanged(
      int player_id, int width, int height) OVERRIDE;
  virtual void OnReadFromDemuxer(int player_id,
                                 media::DemuxerStream::Type type) OVERRIDE;
  virtual void RequestMediaResources(int player_id) OVERRIDE;
  virtual void ReleaseMediaResources(int player_id) OVERRIDE;
  virtual media::MediaResourceGetter* GetMediaResourceGetter() OVERRIDE;
  virtual media::MediaPlayerAndroid* GetFullscreenPlayer() OVERRIDE;
  virtual media::MediaPlayerAndroid* GetPlayer(int player_id) OVERRIDE;
  virtual media::MediaDrmBridge* GetDrmBridge(int media_keys_id) OVERRIDE;
  virtual void DestroyAllMediaPlayers() OVERRIDE;
  virtual void OnMediaSeekRequest(int player_id, base::TimeDelta time_to_seek,
                                  unsigned seek_request_id) OVERRIDE;
  virtual void OnMediaConfigRequest(int player_id) OVERRIDE;
  virtual void OnProtectedSurfaceRequested(int player_id) OVERRIDE;
  virtual void OnKeyAdded(int media_keys_id,
                          const std::string& session_id) OVERRIDE;
  virtual void OnKeyError(int media_keys_id,
                          const std::string& session_id,
                          media::MediaKeys::KeyError error_code,
                          int system_code) OVERRIDE;
  virtual void OnKeyMessage(int media_keys_id,
                            const std::string& session_id,
                            const std::vector<uint8>& message,
                            const std::string& destination_url) OVERRIDE;

#if defined(GOOGLE_TV)
  void AttachExternalVideoSurface(int player_id, jobject surface);
  void DetachExternalVideoSurface(int player_id);
#endif

 protected:
  friend MediaPlayerManager* MediaPlayerManager::Create(
      content::RenderViewHost*);

  // The instance of this class is supposed to be created by either Create()
  // method of MediaPlayerManager or the derived classes constructors.
  explicit BrowserMediaPlayerManager(RenderViewHost* render_view_host);

  // Message handlers.
  virtual void OnEnterFullscreen(int player_id);
  virtual void OnExitFullscreen(int player_id);
  virtual void OnInitialize(
      int player_id,
      const GURL& url,
      media::MediaPlayerAndroid::SourceType source_type,
      const GURL& first_party_for_cookies);
  virtual void OnStart(int player_id);
  virtual void OnSeek(int player_id, base::TimeDelta time);
  virtual void OnPause(int player_id);
  virtual void OnSetVolume(int player_id, double volume);
  virtual void OnReleaseResources(int player_id);
  virtual void OnDestroyPlayer(int player_id);
  virtual void OnDemuxerReady(
      int player_id,
      const media::MediaPlayerHostMsg_DemuxerReady_Params& params);
  virtual void OnReadFromDemuxerAck(
      int player_id,
      const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params);
  void OnMediaSeekRequestAck(int player_id, unsigned seek_request_id);
  void OnInitializeCDM(int media_keys_id, const std::vector<uint8>& uuid);
  void OnGenerateKeyRequest(int media_keys_id,
                            const std::string& type,
                            const std::vector<uint8>& init_data);
  void OnAddKey(int media_keys_id,
                const std::vector<uint8>& key,
                const std::vector<uint8>& init_data,
                const std::string& session_id);
  void OnCancelKeyRequest(int media_keys_id, const std::string& session_id);
  void OnDurationChanged(int player_id, const base::TimeDelta& duration);
  void OnSetMediaKeys(int player_id, int media_keys_id);

#if defined(GOOGLE_TV)
  virtual void OnNotifyExternalSurface(
      int player_id, bool is_request, const gfx::RectF& rect);
#endif

  // Adds a given player to the list.
  void AddPlayer(media::MediaPlayerAndroid* player);

  // Removes the player with the specified id.
  void RemovePlayer(int player_id);

  // Add a new MediaDrmBridge for the given |uuid| and |media_keys_id|.
  void AddDrmBridge(int media_keys_id, const std::vector<uint8>& uuid);

  // Removes the DRM bridge with the specified id.
  void RemoveDrmBridge(int media_keys_id);

 private:
  // An array of managed players.
  ScopedVector<media::MediaPlayerAndroid> players_;

  // An array of managed media DRM bridges.
  ScopedVector<media::MediaDrmBridge> drm_bridges_;

  // The fullscreen video view object or NULL if video is not played in
  // fullscreen.
  scoped_ptr<ContentVideoView> video_view_;

  // Player ID of the fullscreen media player.
  int fullscreen_player_id_;

  WebContents* web_contents_;

  // Object for retrieving resources media players.
  scoped_ptr<media::MediaResourceGetter> media_resource_getter_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMediaPlayerManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_
