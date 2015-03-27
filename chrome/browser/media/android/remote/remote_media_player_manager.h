// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_REMOTE_REMOTE_MEDIA_PLAYER_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ANDROID_REMOTE_REMOTE_MEDIA_PLAYER_MANAGER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/media/android/remote/remote_media_player_bridge.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "media/base/android/media_player_android.h"

struct MediaPlayerHostMsg_Initialize_Params;

namespace remote_media {

// media::MediaPlayerManager implementation that allows the user to play media
// remotely.
class RemoteMediaPlayerManager : public content::BrowserMediaPlayerManager {
 public:
  RemoteMediaPlayerManager(
      content::RenderFrameHost* render_frame_host,
      content::MediaPlayersObserver* audio_monitor);
  ~RemoteMediaPlayerManager() override;

  void OnPlaying(int player_id);
  void OnPaused(int player_id);

  // Callback to trigger when a remote device has been selected.
  void OnRemoteDeviceSelected(int player_id);

  // Callback to trigger when a remote device has been unselected.
  void OnRemoteDeviceUnselected(int player_id);

  // Callback to trigger when the video on a remote device finishes playing.
  void OnRemotePlaybackFinished(int player_id);

  // Callback to trigger when the availability of remote routes changes.
  void OnRouteAvailabilityChanged(int tab_id, bool routes_available);

  void OnMediaMetadataChanged(int player_id,
                              base::TimeDelta duration,
                              int width,
                              int height,
                              bool success) override;

 protected:
  void OnSetPoster(int player_id, const GURL& url) override;

 private:
  // Returns a MediaPlayerAndroid implementation for playing the media remotely.
  RemoteMediaPlayerBridge* CreateRemoteMediaPlayer(
      media::MediaPlayerAndroid* local_player);

  // Replaces the given local player with the remote one. Does nothing if the
  // player is remote already.
  void ReplaceLocalPlayerWithRemote(media::MediaPlayerAndroid* player);

  // Replaces the remote player with the local player this class is holding.
  // Does nothing if there is no remote player.
  void ReplaceRemotePlayerWithLocal();

  // Checks if the URL managed by the player should be played remotely.
  // Returns true if the manager should do nothing, false if it needs to
  // proceed.
  bool MaybeStartPlayingRemotely(int player_id);

  // content::BrowserMediaPlayerManager overrides.
  void OnStart(int player_id) override;
  void OnInitialize(
      const MediaPlayerHostMsg_Initialize_Params& media_player_params) override;
  void OnDestroyPlayer(int player_id) override;
  void OnReleaseResources(int player_id) override;
  void OnRequestRemotePlayback(int player_id) override;
  void OnRequestRemotePlaybackControl(int player_id) override;

  void ReleaseFullscreenPlayer(media::MediaPlayerAndroid* player) override;

  // Callback for when the download of poster image is done.
  void DidDownloadPoster(
      int player_id,
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

  // Return the ID of the tab that's associated with this controller. Returns
  // -1 in case something goes wrong.
  int GetTabId();

  // Get the player id of current remote player, if any, or -1 if none.
  int RemotePlayerId();

  // Get the remote player for a given player id, whether or not it is currently
  // playing remotely.
  RemoteMediaPlayerBridge* GetRemotePlayer(int player_id);

  // The local player that we have replaced with a remote player. This is NULL
  // if we do not have a remote player currently running.
  scoped_ptr<media::MediaPlayerAndroid> replaced_local_player_;

  ScopedVector<RemoteMediaPlayerBridge> remote_players_;

  base::WeakPtrFactory<RemoteMediaPlayerManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaPlayerManager);
};

} // namespace remote_media

#endif  // CHROME_BROWSER_MEDIA_ANDROID_REMOTE_REMOTE_MEDIA_PLAYER_MANAGER_H_
