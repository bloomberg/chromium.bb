// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/browser/media/cdm/browser_cdm_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

#if defined(OS_ANDROID)
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/android/browser_media_session_manager.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/common/media/media_session_messages_android.h"
#include "media/base/android/media_player_android.h"
#endif  // defined(OS_ANDROID)

namespace content {

MediaWebContentsObserver::MediaWebContentsObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

MediaWebContentsObserver::~MediaWebContentsObserver() {}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  ClearPowerSaveBlockers(render_frame_host);

#if defined(OS_ANDROID)
  // Always destroy the media players before CDMs because we do not support
  // detaching CDMs from media players yet. See http://crbug.com/330324
  media_player_managers_.erase(render_frame_host);
  media_session_managers_.erase(render_frame_host);

  // TODO(xhwang): Currently MediaWebContentsObserver, BrowserMediaPlayerManager
  // and BrowserCdmManager all run on browser UI thread. So this call is okay.
  // In the future we need to support the case where MediaWebContentsObserver
  // get notified on browser UI thread, but BrowserMediaPlayerManager and
  // BrowserCdmManager run on a different thread.
  BrowserCdmManager* browser_cdm_manager =
      BrowserCdmManager::FromProcess(render_frame_host->GetProcess()->GetID());
  if (browser_cdm_manager)
    browser_cdm_manager->RenderFrameDeleted(render_frame_host->GetRoutingID());
#endif
}

void MediaWebContentsObserver::MaybeUpdateAudibleState(bool recently_audible) {
  if (recently_audible) {
    if (!audio_power_save_blocker_)
      CreateAudioPowerSaveBlocker();
  } else {
    audio_power_save_blocker_.reset();
  }
}

bool MediaWebContentsObserver::OnMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  if (OnMediaPlayerDelegateMessageReceived(msg, render_frame_host))
    return true;

#if defined(OS_ANDROID)
  if (OnMediaPlayerMessageReceived(msg, render_frame_host))
    return true;

  if (OnMediaPlayerSetCdmMessageReceived(msg, render_frame_host))
    return true;
#endif

  return false;
}

bool MediaWebContentsObserver::OnMediaPlayerDelegateMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  // TODO(dalecurtis): These should no longer be FrameHostMsg.
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MediaWebContentsObserver, msg,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(FrameHostMsg_MediaPlayingNotification,
                        OnMediaPlayingNotification)
    IPC_MESSAGE_HANDLER(FrameHostMsg_MediaPausedNotification,
                        OnMediaPausedNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaWebContentsObserver::OnMediaPlayingNotification(
    RenderFrameHost* render_frame_host,
    int64_t player_cookie,
    bool has_video,
    bool has_audio,
    bool is_remote) {
  // Ignore the videos playing remotely and don't hold the wake lock for the
  // screen. TODO(dalecurtis): Is this correct? It means observers will not
  // receive play and pause messages.
  if (is_remote)
    return;

  const MediaPlayerId id(render_frame_host, player_cookie);
  if (has_audio) {
    AddMediaPlayerEntry(id, &active_audio_players_);

    // If we don't have audio stream monitoring, allocate the audio power save
    // blocker here instead of during NotifyNavigationStateChanged().
    if (!audio_power_save_blocker_ &&
        !AudioStreamMonitor::monitoring_available()) {
      CreateAudioPowerSaveBlocker();
    }
  }

  if (has_video) {
    AddMediaPlayerEntry(id, &active_video_players_);

    // If we're not hidden and have just created a player, create a blocker.
    if (!video_power_save_blocker_ &&
        !static_cast<WebContentsImpl*>(web_contents())->IsHidden()) {
      CreateVideoPowerSaveBlocker();
    }
  }

  // Notify observers of the new player.
  DCHECK(has_audio || has_video);
  static_cast<WebContentsImpl*>(web_contents())->MediaStartedPlaying(id);
}

void MediaWebContentsObserver::OnMediaPausedNotification(
    RenderFrameHost* render_frame_host,
    int64_t player_cookie) {
  const MediaPlayerId id(render_frame_host, player_cookie);
  const bool removed_audio = RemoveMediaPlayerEntry(id, &active_audio_players_);
  const bool removed_video = RemoveMediaPlayerEntry(id, &active_video_players_);
  MaybeReleasePowerSaveBlockers();

  if (removed_audio || removed_video) {
    // Notify observers the player has been "paused".
    static_cast<WebContentsImpl*>(web_contents())->MediaStoppedPlaying(id);
  }
}

void MediaWebContentsObserver::ClearPowerSaveBlockers(
    RenderFrameHost* render_frame_host) {
  std::set<MediaPlayerId> removed_players;
  RemoveAllMediaPlayerEntries(render_frame_host, &active_audio_players_,
                              &removed_players);
  RemoveAllMediaPlayerEntries(render_frame_host, &active_video_players_,
                              &removed_players);
  MaybeReleasePowerSaveBlockers();

  // Notify all observers the player has been "paused".
  WebContentsImpl* wci = static_cast<WebContentsImpl*>(web_contents());
  for (const auto& id : removed_players)
    wci->MediaStoppedPlaying(id);
}

void MediaWebContentsObserver::CreateAudioPowerSaveBlocker() {
  DCHECK(!audio_power_save_blocker_);
  audio_power_save_blocker_ = PowerSaveBlocker::Create(
      PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
      PowerSaveBlocker::kReasonAudioPlayback, "Playing audio");
}

void MediaWebContentsObserver::CreateVideoPowerSaveBlocker() {
  DCHECK(!video_power_save_blocker_);
  DCHECK(!active_video_players_.empty());
  video_power_save_blocker_ = PowerSaveBlocker::Create(
      PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
      PowerSaveBlocker::kReasonVideoPlayback, "Playing video");
// TODO(mfomitchev): Support PowerSaveBlocker on Aura - crbug.com/546718.
#if defined(OS_ANDROID) && !defined(USE_AURA)
  static_cast<PowerSaveBlockerImpl*>(video_power_save_blocker_.get())
      ->InitDisplaySleepBlocker(web_contents());
#endif
}

void MediaWebContentsObserver::WasShown() {
  // Restore power save blocker if there are active video players running.
  if (!active_video_players_.empty() && !video_power_save_blocker_)
    CreateVideoPowerSaveBlocker();
}

void MediaWebContentsObserver::WasHidden() {
  // If there are entities capturing screenshots or video (e.g., mirroring),
  // don't release the power save blocker.
  if (!web_contents()->GetCapturerCount())
    video_power_save_blocker_.reset();
}

void MediaWebContentsObserver::MaybeReleasePowerSaveBlockers() {
  // If there are no more audio players and we don't have audio stream
  // monitoring, release the audio power save blocker here instead of during
  // NotifyNavigationStateChanged().
  if (active_audio_players_.empty() &&
      !AudioStreamMonitor::monitoring_available()) {
    audio_power_save_blocker_.reset();
  }

  // If there are no more video players, clear the video power save blocker.
  if (active_video_players_.empty())
    video_power_save_blocker_.reset();
}

void MediaWebContentsObserver::AddMediaPlayerEntry(
    const MediaPlayerId& id,
    ActiveMediaPlayerMap* player_map) {
  DCHECK(std::find((*player_map)[id.first].begin(),
                   (*player_map)[id.first].end(),
                   id.second) == (*player_map)[id.first].end());
  (*player_map)[id.first].push_back(id.second);
}

bool MediaWebContentsObserver::RemoveMediaPlayerEntry(
    const MediaPlayerId& id,
    ActiveMediaPlayerMap* player_map) {
  auto it = player_map->find(id.first);
  if (it == player_map->end())
    return false;

  // Remove the player.
  auto player_for_removal =
      std::remove(it->second.begin(), it->second.end(), id.second);
  if (player_for_removal == it->second.end())
    return false;
  it->second.erase(player_for_removal);

  // If there are no players left, remove the map entry.
  if (it->second.empty())
    player_map->erase(it);

  return true;
}

void MediaWebContentsObserver::RemoveAllMediaPlayerEntries(
    RenderFrameHost* render_frame_host,
    ActiveMediaPlayerMap* player_map,
    std::set<MediaPlayerId>* removed_players) {
  auto it = player_map->find(render_frame_host);
  if (it == player_map->end())
    return;

  for (int64_t player_cookie : it->second)
    removed_players->insert(MediaPlayerId(render_frame_host, player_cookie));

  player_map->erase(it);
}

#if defined(OS_ANDROID)

bool MediaWebContentsObserver::OnMediaPlayerMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaWebContentsObserver, msg)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_EnterFullscreen,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnEnterFullscreen)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Initialize,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnInitialize)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Start,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnStart)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Seek,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnSeek)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Pause,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnPause)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_SetVolume,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnSetVolume)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_SetPoster,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnSetPoster)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Release,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnReleaseResources)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_DestroyMediaPlayer,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnDestroyPlayer)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_RequestRemotePlayback,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnRequestRemotePlayback)
    IPC_MESSAGE_FORWARD(
        MediaPlayerHostMsg_RequestRemotePlaybackControl,
        GetMediaPlayerManager(render_frame_host),
        BrowserMediaPlayerManager::OnRequestRemotePlaybackControl)
#if defined(VIDEO_HOLE)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_NotifyExternalSurface,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnNotifyExternalSurface)
#endif  // defined(VIDEO_HOLE)
    IPC_MESSAGE_FORWARD(MediaSessionHostMsg_Activate,
                        GetMediaSessionManager(render_frame_host),
                        BrowserMediaSessionManager::OnActivate)
    IPC_MESSAGE_FORWARD(MediaSessionHostMsg_Deactivate,
                        GetMediaSessionManager(render_frame_host),
                        BrowserMediaSessionManager::OnDeactivate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool MediaWebContentsObserver::OnMediaPlayerSetCdmMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(
      MediaWebContentsObserver, msg, render_frame_host)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_SetCdm, OnSetCdm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaWebContentsObserver::OnSetCdm(RenderFrameHost* render_frame_host,
                                        int player_id,
                                        int cdm_id) {
  media::MediaPlayerAndroid* media_player =
      GetMediaPlayerManager(render_frame_host)->GetPlayer(player_id);
  if (!media_player) {
    NOTREACHED() << "OnSetCdm: MediaPlayer not found for " << player_id;
    return;
  }

  // MediaPlayerAndroid runs on the same thread as BrowserCdmManager.
  BrowserCdmManager* browser_cdm_manager =
      BrowserCdmManager::FromProcess(render_frame_host->GetProcess()->GetID());
  if (!browser_cdm_manager) {
    NOTREACHED() << "OnSetCdm: CDM not found for " << cdm_id;
    return;
  }

  scoped_refptr<media::MediaKeys> cdm =
      browser_cdm_manager->GetCdm(render_frame_host->GetRoutingID(), cdm_id);
  if (!cdm) {
    NOTREACHED() << "OnSetCdm: CDM not found for " << cdm_id;
    return;
  }

  // TODO(xhwang): This could possibly fail. In that case we should reject the
  // promise.
  media_player->SetCdm(cdm);
}

BrowserMediaPlayerManager* MediaWebContentsObserver::GetMediaPlayerManager(
    RenderFrameHost* render_frame_host) {
  auto it = media_player_managers_.find(render_frame_host);
  if (it != media_player_managers_.end())
    return it->second;

  BrowserMediaPlayerManager* manager =
      BrowserMediaPlayerManager::Create(render_frame_host);
  media_player_managers_.set(render_frame_host, make_scoped_ptr(manager));
  return manager;
}

BrowserMediaSessionManager* MediaWebContentsObserver::GetMediaSessionManager(
    RenderFrameHost* render_frame_host) {
  auto it = media_session_managers_.find(render_frame_host);
  if (it != media_session_managers_.end())
    return it->second;

  BrowserMediaSessionManager* manager =
      new BrowserMediaSessionManager(render_frame_host);
  media_session_managers_.set(render_frame_host, make_scoped_ptr(manager));
  return manager;
}

#if defined(VIDEO_HOLE)
void MediaWebContentsObserver::OnFrameInfoUpdated() {
  for (auto it = media_player_managers_.begin();
       it != media_player_managers_.end(); ++it) {
    it->second->OnFrameInfoUpdated();
  }
}
#endif  // defined(VIDEO_HOLE)

#endif  // defined(OS_ANDROID)

}  // namespace content
