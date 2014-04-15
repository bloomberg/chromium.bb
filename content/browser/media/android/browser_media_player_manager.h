// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "content/browser/android/content_video_view.h"
#include "content/common/media/cdm_messages_enums.h"
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
class ContentViewCoreImpl;
class ExternalVideoSurfaceContainer;
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

  ContentViewCoreImpl* GetContentViewCore() const;

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
  virtual media::MediaResourceGetter* GetMediaResourceGetter() OVERRIDE;
  virtual media::MediaPlayerAndroid* GetFullscreenPlayer() OVERRIDE;
  virtual media::MediaPlayerAndroid* GetPlayer(int player_id) OVERRIDE;
  virtual media::MediaDrmBridge* GetDrmBridge(int cdm_id) OVERRIDE;
  virtual void DestroyAllMediaPlayers() OVERRIDE;
  virtual void OnProtectedSurfaceRequested(int player_id) OVERRIDE;
  virtual void OnSessionCreated(int cdm_id,
                                uint32 session_id,
                                const std::string& web_session_id) OVERRIDE;
  virtual void OnSessionMessage(int cdm_id,
                                uint32 session_id,
                                const std::vector<uint8>& message,
                                const GURL& destination_url) OVERRIDE;
  virtual void OnSessionReady(int cdm_id, uint32 session_id) OVERRIDE;
  virtual void OnSessionClosed(int cdm_id, uint32 session_id) OVERRIDE;
  virtual void OnSessionError(int cdm_id,
                              uint32 session_id,
                              media::MediaKeys::KeyError error_code,
                              uint32 system_code) OVERRIDE;

#if defined(VIDEO_HOLE)
  void AttachExternalVideoSurface(int player_id, jobject surface);
  void DetachExternalVideoSurface(int player_id);
  void OnFrameInfoUpdated();
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
  virtual void OnSetPoster(int player_id, const GURL& poster);
  virtual void OnReleaseResources(int player_id);
  virtual void OnDestroyPlayer(int player_id);
  virtual void ReleaseFullscreenPlayer(media::MediaPlayerAndroid* player);
  void OnInitializeCdm(int cdm_id,
                       const std::string& key_system,
                       const GURL& frame_url);
  void OnCreateSession(int cdm_id,
                       uint32 session_id,
                       CdmHostMsg_CreateSession_ContentType content_type,
                       const std::vector<uint8>& init_data);
  void OnUpdateSession(int cdm_id,
                       uint32 session_id,
                       const std::vector<uint8>& response);
  void OnReleaseSession(int cdm_id, uint32 session_id);
  void OnSetCdm(int player_id, int cdm_id);
  void OnDestroyCdm(int cdm_id);

  // Cancels all pending session creations associated with |cdm_id|.
  void CancelAllPendingSessionCreations(int cdm_id);

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

  // Adds a new MediaDrmBridge for the given |key_system|, |cdm_id|, and
  // |frame_url|.
  void AddDrmBridge(int cdm_id,
                    const std::string& key_system,
                    const GURL& frame_url);

  // Removes the DRM bridge with the specified id.
  void RemoveDrmBridge(int cdm_id);

 private:
  // If |permitted| is false, it does nothing but send
  // |CdmMsg_SessionError| IPC message.
  // The primary use case is infobar permission callback, i.e., when infobar
  // can decide user's intention either from interacting with the actual info
  // bar or from the saved preference.
  void CreateSessionIfPermitted(int cdm_id,
                                uint32 session_id,
                                const std::string& content_type,
                                const std::vector<uint8>& init_data,
                                bool permitted);

  // Constructs a MediaPlayerAndroid object.
  media::MediaPlayerAndroid* CreateMediaPlayer(
      MediaPlayerHostMsg_Initialize_Type type,
      int player_id,
      const GURL& url,
      const GURL& first_party_for_cookies,
      int demuxer_client_id,
      bool hide_url_log,
      media::MediaPlayerManager* manager,
      BrowserDemuxerAndroid* demuxer);

  // MediaPlayerAndroid must call this before it is going to decode
  // media streams. This helps the manager object maintain an array
  // of active MediaPlayerAndroid objects and release the resources
  // when needed. Currently we only count video resources as they are
  // constrained by hardware and memory limits.
  virtual void OnMediaResourcesRequested(int player_id);

  // Similar to the above call, MediaPlayerAndroid must call this method when
  // releasing all the decoding resources.
  virtual void OnMediaResourcesReleased(int player_id);

#if defined(VIDEO_HOLE)
  void OnNotifyExternalSurface(
      int player_id, bool is_request, const gfx::RectF& rect);
  void OnRequestExternalSurface(int player_id, const gfx::RectF& rect);
#endif  // defined(VIDEO_HOLE)

  // An array of managed players.
  ScopedVector<media::MediaPlayerAndroid> players_;

  // An array of managed media DRM bridges.
  ScopedVector<media::MediaDrmBridge> drm_bridges_;

  // A set of media keys IDs that are pending approval or approved to access
  // device DRM credentials.
  // These 2 sets does not cover all the EME videos. If a video only streams
  // clear data, it will not be included in either set.
  std::set<int> cdm_ids_pending_approval_;
  std::set<int> cdm_ids_approved_;

  // The fullscreen video view object or NULL if video is not played in
  // fullscreen.
  scoped_ptr<ContentVideoView> video_view_;

#if defined(VIDEO_HOLE)
  scoped_ptr<ExternalVideoSurfaceContainer> external_video_surface_container_;
#endif

  // Player ID of the fullscreen media player.
  int fullscreen_player_id_;

  // The player ID pending to enter fullscreen.
  int pending_fullscreen_player_id_;

  // Whether the fullscreen player has been Release()-d.
  bool fullscreen_player_is_released_;

  WebContents* web_contents_;

  // Object for retrieving resources media players.
  scoped_ptr<media::MediaResourceGetter> media_resource_getter_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<BrowserMediaPlayerManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMediaPlayerManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_
