// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/browser/media/audible_metrics.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/power_save_blocker_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

namespace content {

namespace {

static base::LazyInstance<AudibleMetrics>::Leaky g_audible_metrics =
    LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

MediaWebContentsObserver::MediaWebContentsObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

MediaWebContentsObserver::~MediaWebContentsObserver() {}

void MediaWebContentsObserver::WebContentsDestroyed() {
  g_audible_metrics.Get().UpdateAudibleWebContentsState(web_contents(), false);
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  ClearPowerSaveBlockers(render_frame_host);
}

void MediaWebContentsObserver::MaybeUpdateAudibleState() {
  if (!AudioStreamMonitor::monitoring_available())
    return;

  AudioStreamMonitor* audio_stream_monitor =
      static_cast<WebContentsImpl*>(web_contents())->audio_stream_monitor();

  if (audio_stream_monitor->WasRecentlyAudible()) {
    if (!audio_power_save_blocker_)
      CreateAudioPowerSaveBlocker();
  } else {
    audio_power_save_blocker_.reset();
  }

  g_audible_metrics.Get().UpdateAudibleWebContentsState(
      web_contents(), audio_stream_monitor->IsCurrentlyAudible());
}

bool MediaWebContentsObserver::OnMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  // TODO(dalecurtis): These should no longer be FrameHostMsg.
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MediaWebContentsObserver, msg,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaDestroyed,
                        OnMediaDestroyed)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaPaused, OnMediaPaused)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaPlaying,
                        OnMediaPlaying)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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

void MediaWebContentsObserver::OnMediaDestroyed(
    RenderFrameHost* render_frame_host,
    int delegate_id) {
  OnMediaPaused(render_frame_host, delegate_id, true);
}


void MediaWebContentsObserver::OnMediaPaused(RenderFrameHost* render_frame_host,
                                             int delegate_id,
                                             bool reached_end_of_stream) {
  const MediaPlayerId id(render_frame_host, delegate_id);
  const bool removed_audio = RemoveMediaPlayerEntry(id, &active_audio_players_);
  const bool removed_video = RemoveMediaPlayerEntry(id, &active_video_players_);
  MaybeReleasePowerSaveBlockers();

  if (removed_audio || removed_video) {
    // Notify observers the player has been "paused".
    static_cast<WebContentsImpl*>(web_contents())->MediaStoppedPlaying(id);
  }
}

void MediaWebContentsObserver::OnMediaPlaying(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool has_video,
    bool has_audio,
    bool is_remote,
    base::TimeDelta duration) {
  // Ignore the videos playing remotely and don't hold the wake lock for the
  // screen. TODO(dalecurtis): Is this correct? It means observers will not
  // receive play and pause messages.
  if (is_remote)
    return;

  const MediaPlayerId id(render_frame_host, delegate_id);
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
#if defined(OS_ANDROID)
  static_cast<PowerSaveBlockerImpl*>(video_power_save_blocker_.get())
      ->InitDisplaySleepBlocker(web_contents());
#endif
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

  for (int delegate_id : it->second)
    removed_players->insert(MediaPlayerId(render_frame_host, delegate_id));

  player_map->erase(it);
}

}  // namespace content
