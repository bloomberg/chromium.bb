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
    content::RenderFrameHost* render_frame_host)
    : BrowserMediaPlayerManager(render_frame_host),
      weak_ptr_factory_(this) {
}

RemoteMediaPlayerManager::~RemoteMediaPlayerManager() {}

void RemoteMediaPlayerManager::OnStart(int player_id) {
  RemoteMediaPlayerBridge* remote_player = GetRemotePlayer(player_id);
  if (remote_player) {
    if (IsPlayingRemotely(player_id)) {
      remote_player->Start();
    } else if (remote_player->TakesOverCastDevice()) {
      return;
    }
  }
  BrowserMediaPlayerManager::OnStart(player_id);
}

void RemoteMediaPlayerManager::OnInitialize(
    const MediaPlayerHostMsg_Initialize_Params& media_params) {
  BrowserMediaPlayerManager::OnInitialize(media_params);

  MediaPlayerAndroid* player = GetPlayer(media_params.player_id);
  if (player) {
    RemoteMediaPlayerBridge* remote_player = CreateRemoteMediaPlayer(player);
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
  if (!IsPlayingRemotely(player_id))
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
  alternative_players_.push_back(player);
  player->Initialize();
  return player;
}

// OnSuspend and OnResume are called when the local player loses or gains
// audio focus. If we are playing remotely then ignore these.
void RemoteMediaPlayerManager::OnSuspend(int player_id) {
  if (!IsPlayingRemotely(player_id))
    BrowserMediaPlayerManager::OnSuspend(player_id);
}

void RemoteMediaPlayerManager::OnResume(int player_id) {
  if (!IsPlayingRemotely(player_id))
    BrowserMediaPlayerManager::OnResume(player_id);
}

void RemoteMediaPlayerManager::SwapCurrentPlayer(int player_id) {
  // Find the remote player
  auto it = GetAlternativePlayer(player_id);
  if (it == alternative_players_.end())
    return;
  MediaPlayerAndroid* new_player = *it;
  scoped_ptr<MediaPlayerAndroid> old_player = SwapPlayer(player_id, new_player);
  alternative_players_.weak_erase(it);
  alternative_players_.push_back(old_player.release());
}

void RemoteMediaPlayerManager::SwitchToRemotePlayer(
    int player_id,
    const std::string& casting_message) {
  DCHECK(!IsPlayingRemotely(player_id));
  SwapCurrentPlayer(player_id);
  players_playing_remotely_.insert(player_id);
  Send(new MediaPlayerMsg_DidMediaPlayerPlay(RoutingID(), player_id));
  Send(new MediaPlayerMsg_ConnectedToRemoteDevice(RoutingID(), player_id,
                                                  casting_message));
}

void RemoteMediaPlayerManager::SwitchToLocalPlayer(int player_id) {
  DCHECK(IsPlayingRemotely(player_id));
  SwapCurrentPlayer(player_id);
  players_playing_remotely_.erase(player_id);
  Send(new MediaPlayerMsg_DisconnectedFromRemoteDevice(RoutingID(), player_id));
}

void RemoteMediaPlayerManager::ReplaceRemotePlayerWithLocal(int player_id) {
  if (!IsPlayingRemotely(player_id))
    return;
  MediaPlayerAndroid* remote_player = GetPlayer(player_id);
  remote_player->Pause(true);
  Send(new MediaPlayerMsg_DidMediaPlayerPause(RoutingID(), player_id));
  Send(new MediaPlayerMsg_DisconnectedFromRemoteDevice(RoutingID(), player_id));

  SwapCurrentPlayer(player_id);
  GetLocalPlayer(player_id)->SeekTo(remote_player->GetCurrentTime());
  remote_player->Release();
  players_playing_remotely_.erase(player_id);
}

void RemoteMediaPlayerManager::OnRemoteDeviceUnselected(int player_id) {
  ReplaceRemotePlayerWithLocal(player_id);
}

void RemoteMediaPlayerManager::OnRemotePlaybackFinished(int player_id) {
  ReplaceRemotePlayerWithLocal(player_id);
}

void RemoteMediaPlayerManager::OnRouteAvailabilityChanged(
    int player_id, bool routes_available) {
  Send(
      new MediaPlayerMsg_RemoteRouteAvailabilityChanged(RoutingID(), player_id,
                                                        routes_available));
}

void RemoteMediaPlayerManager::ReleaseFullscreenPlayer(
    MediaPlayerAndroid* player) {
  int player_id = player->player_id();
  // Release the original player's resources, not the current fullscreen player
  // (which is the remote player).
  if (IsPlayingRemotely(player_id))
    GetLocalPlayer(player_id)->Release();
  else
    BrowserMediaPlayerManager::ReleaseFullscreenPlayer(player);
}

void RemoteMediaPlayerManager::OnPlaying(int player_id) {
  Send(new MediaPlayerMsg_DidMediaPlayerPlay(RoutingID(),player_id));
}

void RemoteMediaPlayerManager::OnPaused(int player_id) {
  Send(new MediaPlayerMsg_DidMediaPlayerPause(RoutingID(),player_id));
}

ScopedVector<MediaPlayerAndroid>::iterator
RemoteMediaPlayerManager::GetAlternativePlayer(int player_id) {
  for (auto it = alternative_players_.begin(); it != alternative_players_.end();
       ++it) {
    if ((*it)->player_id() == player_id) {
      return it;
    }
  }
  return alternative_players_.end();
}

RemoteMediaPlayerBridge* RemoteMediaPlayerManager::GetRemotePlayer(
    int player_id) {
  if (IsPlayingRemotely(player_id))
    return static_cast<RemoteMediaPlayerBridge*>(GetPlayer(player_id));
  auto it = GetAlternativePlayer(player_id);
  if (it == alternative_players_.end())
    return nullptr;
  return static_cast<RemoteMediaPlayerBridge*>(*it);
}

MediaPlayerAndroid* RemoteMediaPlayerManager::GetLocalPlayer(int player_id) {
  if (!IsPlayingRemotely(player_id))
    return static_cast<RemoteMediaPlayerBridge*>(GetPlayer(player_id));
  auto it = GetAlternativePlayer(player_id);
  if (it == alternative_players_.end())
    return nullptr;
  return *it;
}

void RemoteMediaPlayerManager::OnMediaMetadataChanged(int player_id,
                                                      base::TimeDelta duration,
                                                      int width,
                                                      int height,
                                                      bool success) {
  if (IsPlayingRemotely(player_id)) {
    MediaPlayerAndroid* local_player = GetLocalPlayer(player_id);
    Send(new MediaPlayerMsg_MediaMetadataChanged(
        RoutingID(), player_id, duration, local_player->GetVideoWidth(),
        local_player->GetVideoHeight(), success));
  } else {
    BrowserMediaPlayerManager::OnMediaMetadataChanged(player_id, duration,
                                                      width, height, success);
  }
}

bool RemoteMediaPlayerManager::IsPlayingRemotely(int player_id) {
  return players_playing_remotely_.count(player_id) != 0;
}

} // namespace remote_media
