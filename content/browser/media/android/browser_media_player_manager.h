// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "content/browser/android/content_video_view.h"
#include "content/common/media/media_player_messages_enums_android.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/android/media_player_android.h"
#include "media/base/android/media_player_manager.h"
#include "ui/gfx/rect_f.h"
#include "url/gurl.h"

namespace media {
class DemuxerAndroid;
class MediaDrmBridge;
}

namespace content {
class BrowserDemuxerAndroid;
class WebContents;

// This class manages all the MediaPlayerAndroid objects. It receives
// control operations from the the render process, and forwards
// them to corresponding MediaPlayerAndroid object. Callbacks from
// MediaPlayerAndroid objects are converted to IPCs and then sent to the
// render process.
class CONTENT_EXPORT BrowserMediaPlayerManager
    : public WebContentsObserver,
      public media::MediaPlayerManager {
 public:
  // Permits embedders to provide an extended version of the class.
  typedef BrowserMediaPlayerManager* (*Factory)(RenderViewHost*);
  static void RegisterFactory(Factory factory);

  // Returns a new instance using the registered factory if available.
  static BrowserMediaPlayerManager* Create(RenderViewHost* rvh);

  virtual ~BrowserMediaPlayerManager();

  // WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Fullscreen video playback controls.
  virtual void FullscreenPlayerPlay();
  virtual void FullscreenPlayerPause();
  virtual void FullscreenPlayerSeek(int msec);
  virtual void ExitFullscreen(bool release_media_player);
  virtual void SetVideoSurface(gfx::ScopedJavaSurface surface);

  // Called when browser player wants the renderer media element to seek.
  // Any actual seek started by renderer will be handled by browser in OnSeek().
  void OnSeekRequest(int player_id, const base::TimeDelta& time_to_seek);

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
      int player_id,
      const base::TimeDelta& current_time) OVERRIDE;
  virtual void OnError(int player_id, int error) OVERRIDE;
  virtual void OnVideoSizeChanged(
      int player_id, int width, int height) OVERRIDE;
  virtual void RequestMediaResources(int player_id) OVERRIDE;
  virtual void ReleaseMediaResources(int player_id) OVERRIDE;
  virtual media::MediaResourceGetter* GetMediaResourceGetter() OVERRIDE;
  virtual media::MediaPlayerAndroid* GetFullscreenPlayer() OVERRIDE;
  virtual media::MediaPlayerAndroid* GetPlayer(int player_id) OVERRIDE;
  virtual media::MediaDrmBridge* GetDrmBridge(int media_keys_id) OVERRIDE;
  virtual void DestroyAllMediaPlayers() OVERRIDE;
  virtual void OnProtectedSurfaceRequested(int player_id) OVERRIDE;
  virtual void OnSessionCreated(int media_keys_id,
                                uint32 session_id,
                                const std::string& web_session_id) OVERRIDE;
  virtual void OnSessionMessage(int media_keys_id,
                                uint32 session_id,
                                const std::vector<uint8>& message,
                                const std::string& destination_url) OVERRIDE;
  virtual void OnSessionReady(int media_keys_id, uint32 session_id) OVERRIDE;
  virtual void OnSessionClosed(int media_keys_id, uint32 session_id) OVERRIDE;
  virtual void OnSessionError(int media_keys_id,
                              uint32 session_id,
                              media::MediaKeys::KeyError error_code,
                              int system_code) OVERRIDE;

#if defined(VIDEO_HOLE)
  void AttachExternalVideoSurface(int player_id, jobject surface);
  void DetachExternalVideoSurface(int player_id);
#endif  // defined(VIDEO_HOLE)

  // Called to disble the current fullscreen playback if the video is encrypted.
  // TODO(qinmin): remove this once we have the new fullscreen mode.
  void DisableFullscreenEncryptedMediaPlayback();

 protected:
  // Clients must use Create() or subclass constructor.
  explicit BrowserMediaPlayerManager(RenderViewHost* render_view_host);

  // Message handlers.
  virtual void OnEnterFullscreen(int player_id);
  virtual void OnExitFullscreen(int player_id);
  virtual void OnInitialize(
      MediaPlayerHostMsg_Initialize_Type type,
      int player_id,
      const GURL& url,
      const GURL& first_party_for_cookies,
      int demuxer_client_id);
  virtual void OnStart(int player_id);
  virtual void OnSeek(int player_id, const base::TimeDelta& time);
  virtual void OnPause(int player_id, bool is_media_related_action);
  virtual void OnSetVolume(int player_id, double volume);
  virtual void OnReleaseResources(int player_id);
  virtual void OnDestroyPlayer(int player_id);
  void OnInitializeCDM(int media_keys_id,
                       const std::vector<uint8>& uuid,
                       const GURL& frame_url);
  void OnCreateSession(int media_keys_id,
                       uint32 session_id,
                       const std::string& type,
                       const std::vector<uint8>& init_data);
  void OnUpdateSession(int media_keys_id,
                       uint32 session_id,
                       const std::vector<uint8>& response);
  void OnReleaseSession(int media_keys_id, uint32 session_id);
  void OnSetMediaKeys(int player_id, int media_keys_id);

#if defined(VIDEO_HOLE)
  virtual void OnNotifyExternalSurface(
      int player_id, bool is_request, const gfx::RectF& rect);
#endif  // defined(VIDEO_HOLE)

  // Adds a given player to the list.
  void AddPlayer(media::MediaPlayerAndroid* player);

  // Removes the player with the specified id.
  void RemovePlayer(int player_id);

  // Replaces a player with the specified id with a given MediaPlayerAndroid
  // object. This will also return the original MediaPlayerAndroid object that
  // was replaced.
  scoped_ptr<media::MediaPlayerAndroid> SwapPlayer(
      int player_id,
      media::MediaPlayerAndroid* player);

  // Adds a new MediaDrmBridge for the given |uuid|, |media_keys_id|, and
  // |frame_url|.
  void AddDrmBridge(int media_keys_id,
                    const std::vector<uint8>& uuid,
                    const GURL& frame_url);

  // Removes the DRM bridge with the specified id.
  void RemoveDrmBridge(int media_keys_id);

 private:
  void GenerateKeyIfAllowed(int media_keys_id,
                            uint32 session_id,
                            const std::string& type,
                            const std::vector<uint8>& init_data,
                            bool allowed);

  // Constructs a MediaPlayerAndroid object. Declared static to permit embedders
  // to override functionality.
  //
  // Objects must call |manager->RequestMediaResources()| before decoding
  // and |manager->ReleaseMediaSources()| after finishing. This allows the
  // manager to track decoding resources across the process and free them as
  // needed.
  static media::MediaPlayerAndroid* CreateMediaPlayer(
      MediaPlayerHostMsg_Initialize_Type type,
      int player_id,
      const GURL& url,
      const GURL& first_party_for_cookies,
      int demuxer_client_id,
      bool hide_url_log,
      media::MediaPlayerManager* manager,
      BrowserDemuxerAndroid* demuxer);

  // An array of managed players.
  ScopedVector<media::MediaPlayerAndroid> players_;

  // An array of managed media DRM bridges.
  ScopedVector<media::MediaDrmBridge> drm_bridges_;

  // a set of media keys IDs that are pending approval or approved to access
  // device DRM credentials.
  // These 2 sets does not cover all the EME videos. If a video only streams
  // clear data, it will not be included in either set.
  std::set<int> media_keys_ids_pending_approval_;
  std::set<int> media_keys_ids_approved_;

  // The fullscreen video view object or NULL if video is not played in
  // fullscreen.
  scoped_ptr<ContentVideoView> video_view_;

  // Player ID of the fullscreen media player.
  int fullscreen_player_id_;

  // The player ID pending to enter fullscreen.
  int pending_fullscreen_player_id_;

  // Whether the fullscreen player has been Release()-d.
  bool fullscreen_player_is_released_;

  WebContents* web_contents_;

  // Object for retrieving resources media players.
  scoped_ptr<media::MediaResourceGetter> media_resource_getter_;

  base::WeakPtrFactory<BrowserMediaPlayerManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMediaPlayerManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_
