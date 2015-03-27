// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/remote/remote_media_player_manager.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/common/chrome_content_client.h"
#include "content/common/media/media_player_messages_android.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

using media::MediaPlayerAndroid;

namespace remote_media {

RemoteMediaPlayerManager::RemoteMediaPlayerManager(
    content::RenderFrameHost* render_frame_host,
    content::MediaPlayersObserver* audio_monitor)
    : BrowserMediaPlayerManager(render_frame_host, audio_monitor),
      weak_ptr_factory_(this) {
}

RemoteMediaPlayerManager::~RemoteMediaPlayerManager() {}

void RemoteMediaPlayerManager::OnStart(int player_id) {
  // TODO(aberent) This assumes this is the first time we have started this
  // video, rather than restarting after pause. There is a lot of logic here
  // that is unnecessary if we are restarting after pause.
  if (MaybeStartPlayingRemotely(player_id))
    return;

  ReplaceRemotePlayerWithLocal();
  BrowserMediaPlayerManager::OnStart(player_id);
}

void RemoteMediaPlayerManager::OnInitialize(
    const MediaPlayerHostMsg_Initialize_Params& media_params) {
  BrowserMediaPlayerManager::OnInitialize(media_params);

  MediaPlayerAndroid* player = GetPlayer(media_params.player_id);
  if (player) {
    CreateRemoteMediaPlayer(player);
    RemoteMediaPlayerBridge* remote_player = GetRemotePlayer(
        media_params.player_id);
    if (remote_player)
      remote_player->OnPlayerCreated();
  }
}

void RemoteMediaPlayerManager::OnDestroyPlayer(int player_id) {
  RemoteMediaPlayerBridge* player = GetRemotePlayer(player_id);
  if (player)
    player->OnPlayerDestroyed();
  BrowserMediaPlayerManager::OnDestroyPlayer(player_id);
}

void RemoteMediaPlayerManager::OnReleaseResources(int player_id) {
  // We only want to release resources of local players.
  if (player_id != RemotePlayerId())
    BrowserMediaPlayerManager::OnReleaseResources(player_id);
}

void RemoteMediaPlayerManager::OnRequestRemotePlayback(int player_id) {
  RemoteMediaPlayerBridge* player = GetRemotePlayer(player_id);
  if (player)
    player->RequestRemotePlayback();
}

void RemoteMediaPlayerManager::OnRequestRemotePlaybackControl(int player_id) {
  RemoteMediaPlayerBridge* player = GetRemotePlayer(player_id);
  if (player)
    player->RequestRemotePlaybackControl();
}

int RemoteMediaPlayerManager::GetTabId() {
  if (!web_contents())
    return -1;

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents());
  if (!tab)
    return -1;

  return tab->GetAndroidId();
}

void RemoteMediaPlayerManager::OnSetPoster(int player_id, const GURL& url) {
  RemoteMediaPlayerBridge* player = GetRemotePlayer(player_id);

  if (player && url.is_empty()) {
    player->SetPosterBitmap(std::vector<SkBitmap>());
  } else {
    // TODO(aberent) OnSetPoster is called when the attributes of the video
    // element are parsed, which may be before OnInitialize is called. We are
    // here relying on the image fetch taking longer than the delay until
    // OnInitialize is called, and hence the player is created. This is not
    // guaranteed.
    content::WebContents::ImageDownloadCallback callback = base::Bind(
        &RemoteMediaPlayerManager::DidDownloadPoster,
        weak_ptr_factory_.GetWeakPtr(), player_id);
    web_contents()->DownloadImage(
        url,
        false,  // is_favicon, false so that cookies will be used.
        0,  // max_bitmap_size, 0 means no limit.
        false, // normal cache policy.
        callback);
  }
}

void RemoteMediaPlayerManager::DidDownloadPoster(
    int player_id,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  RemoteMediaPlayerBridge* player = GetRemotePlayer(player_id);
  if (player)
    player->SetPosterBitmap(bitmaps);
}

RemoteMediaPlayerBridge* RemoteMediaPlayerManager::CreateRemoteMediaPlayer(
     MediaPlayerAndroid* local_player) {
  RemoteMediaPlayerBridge* player = new RemoteMediaPlayerBridge(
      local_player,
      GetUserAgent(),
      false,
      this);
  remote_players_.push_back(player);
  player->Initialize();
  return player;
}

int RemoteMediaPlayerManager::RemotePlayerId() {
  // The remote player is created with the same id as the corresponding local
  // player.
  if (replaced_local_player_.get())
    return replaced_local_player_->player_id();
  else
    return -1;
}

void RemoteMediaPlayerManager::ReplaceLocalPlayerWithRemote(
    MediaPlayerAndroid* player) {
  if (!player)
    return;

  int player_id = player->player_id();
  if (player_id == RemotePlayerId()) {
    // The player is already remote.
    return;
  }

  // Before we replace the new remote player, put the old local player back
  // in its place.
  ReplaceRemotePlayerWithLocal();

  // Pause the local player first before replacing it. This will allow the local
  // player to reset its state, such as the PowerSaveBlocker.
  // We have to pause locally as well as telling the renderer to pause, because
  // by the time the renderer comes back to us telling us to pause we will have
  // switched players.
  player->Pause(true);
  Send(new MediaPlayerMsg_DidMediaPlayerPause(RoutingID(), player_id));

  // Find the remote player
  for (auto it = remote_players_.begin(); it != remote_players_.end(); ++it) {
    if ((*it)->player_id() == player_id) {
      replaced_local_player_ = SwapPlayer(player_id, *it);

      // Seek to the previous player's position.
      (*it)->SeekTo(player->GetCurrentTime());

      // SwapPlayers takes ownership, so we have to remove the remote player
      // from the vector.
      remote_players_.weak_erase(it);
      break;
    }
  }
}

void RemoteMediaPlayerManager::ReplaceRemotePlayerWithLocal() {
  int player_id = RemotePlayerId();
  if (player_id == -1)
    return;

  Send(new MediaPlayerMsg_DidMediaPlayerPause(RoutingID(), player_id));
  Send(new MediaPlayerMsg_DisconnectedFromRemoteDevice(RoutingID(), player_id));

  scoped_ptr<MediaPlayerAndroid> remote_player =
      SwapPlayer(player_id, replaced_local_player_.release());
  if (remote_player) {
    // Seek to the previous player's position.
    GetPlayer(player_id)->SeekTo(remote_player->GetCurrentTime());

    remote_player->Release();
    // Add the remote player back into the list
    remote_players_.push_back(
        static_cast<RemoteMediaPlayerBridge *>(remote_player.release()));
  }
}

bool RemoteMediaPlayerManager::MaybeStartPlayingRemotely(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (!player)
    return false;

  RemoteMediaPlayerBridge* remote_player = GetRemotePlayer(player_id);

  if (!remote_player)
    return false;

  if (remote_player->IsMediaPlayableRemotely() &&
      remote_player->IsRemotePlaybackAvailable() &&
      remote_player->IsRemotePlaybackPreferredForFrame()) {
    ReplaceLocalPlayerWithRemote(player);

    remote_player->SetNativePlayer();
    remote_player->Start();

    Send(new MediaPlayerMsg_DidMediaPlayerPlay(RoutingID(), player_id));

    Send(new MediaPlayerMsg_ConnectedToRemoteDevice(
        RoutingID(),
        player_id,
        remote_player->GetCastingMessage()));

    return true;
  }

  return false;
}

void RemoteMediaPlayerManager::OnRemoteDeviceSelected(int player_id) {

  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (!player)
    return;

  if (MaybeStartPlayingRemotely(player_id))
    return;
  OnStart(player_id);
}

void RemoteMediaPlayerManager::OnRemoteDeviceUnselected(int player_id) {
  if (player_id == RemotePlayerId())
    ReplaceRemotePlayerWithLocal();
}

void RemoteMediaPlayerManager::OnRemotePlaybackFinished(int player_id) {
  if (player_id == RemotePlayerId())
    ReplaceRemotePlayerWithLocal();
}

void RemoteMediaPlayerManager::OnRouteAvailabilityChanged(
    int player_id, bool routes_available) {
  Send(
      new MediaPlayerMsg_RemoteRouteAvailabilityChanged(RoutingID(), player_id,
                                                        routes_available));
}

void RemoteMediaPlayerManager::ReleaseFullscreenPlayer(
    MediaPlayerAndroid* player) {
  // Release the original player's resources, not the current fullscreen player
  // (which is the remote player).
  if (replaced_local_player_.get())
    replaced_local_player_->Release();
  else
    BrowserMediaPlayerManager::ReleaseFullscreenPlayer(player);
}

void RemoteMediaPlayerManager::OnPlaying(int player_id) {
  Send(new MediaPlayerMsg_DidMediaPlayerPlay(RoutingID(),player_id));
}

void RemoteMediaPlayerManager::OnPaused(int player_id) {
  Send(new MediaPlayerMsg_DidMediaPlayerPause(RoutingID(),player_id));
}

RemoteMediaPlayerBridge* RemoteMediaPlayerManager::GetRemotePlayer(
    int player_id) {
  if (player_id == RemotePlayerId()) {
    return static_cast<RemoteMediaPlayerBridge*>(GetPlayer(player_id));
  } else {
    for (RemoteMediaPlayerBridge* player : remote_players_) {
      if (player->player_id() == player_id) {
        return player;
      }
    }
    return nullptr;
  }
}

void RemoteMediaPlayerManager::OnMediaMetadataChanged(int player_id,
                                                      base::TimeDelta duration,
                                                      int width,
                                                      int height,
                                                      bool success) {
  if (player_id == RemotePlayerId() && replaced_local_player_) {
    Send(new MediaPlayerMsg_MediaMetadataChanged(
        RoutingID(), player_id, duration,
        replaced_local_player_->GetVideoWidth(),
        replaced_local_player_->GetVideoHeight(), success));
  } else {
    BrowserMediaPlayerManager::OnMediaMetadataChanged(player_id, duration,
                                                      width, height, success);
  }
}

} // namespace remote_media
