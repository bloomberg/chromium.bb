// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include <memory>

#include "build/build_config.h"
#include "content/browser/media/audible_metrics.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/wake_lock/public/interfaces/wake_lock_context.mojom.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

AudibleMetrics* GetAudibleMetrics() {
  static AudibleMetrics* metrics = new AudibleMetrics();
  return metrics;
}

}  // anonymous namespace

MediaWebContentsObserver::MediaWebContentsObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      has_audio_wake_lock_for_testing_(false),
      has_video_wake_lock_for_testing_(false),
      session_controllers_manager_(this) {}

MediaWebContentsObserver::~MediaWebContentsObserver() = default;

void MediaWebContentsObserver::WebContentsDestroyed() {
  GetAudibleMetrics()->UpdateAudibleWebContentsState(web_contents(), false);
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  ClearWakeLocks(render_frame_host);
  session_controllers_manager_.RenderFrameDeleted(render_frame_host);

  if (fullscreen_player_ && fullscreen_player_->first == render_frame_host)
    fullscreen_player_.reset();
}

void MediaWebContentsObserver::MaybeUpdateAudibleState() {
  AudioStreamMonitor* audio_stream_monitor =
      web_contents_impl()->audio_stream_monitor();

  if (audio_stream_monitor->WasRecentlyAudible())
    LockAudio();
  else
    CancelAudioLock();

  GetAudibleMetrics()->UpdateAudibleWebContentsState(
      web_contents(), audio_stream_monitor->IsCurrentlyAudible());
}

bool MediaWebContentsObserver::HasActiveEffectivelyFullscreenVideo() const {
  if (!web_contents()->IsFullscreen() || !fullscreen_player_)
    return false;

  // Check that the player is active.
  const auto& players = active_video_players_.find(fullscreen_player_->first);
  if (players == active_video_players_.end())
    return false;
  if (players->second.find(fullscreen_player_->second) == players->second.end())
    return false;

  return true;
}

bool MediaWebContentsObserver::OnMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MediaWebContentsObserver, msg,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaDestroyed,
                        OnMediaDestroyed)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaPaused, OnMediaPaused)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaPlaying,
                        OnMediaPlaying)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMutedStatusChanged,
                        OnMediaMutedStatusChanged)
    IPC_MESSAGE_HANDLER(
        MediaPlayerDelegateHostMsg_OnMediaEffectivelyFullscreenChanged,
        OnMediaEffectivelyFullscreenChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaSizeChanged,
                        OnMediaSizeChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaWebContentsObserver::WasShown() {
  // Restore wake lock if there are active video players running.
  if (!active_video_players_.empty())
    LockVideo();
}

void MediaWebContentsObserver::WasHidden() {
  // If there are entities capturing screenshots or video (e.g., mirroring),
  // don't release the wake lock.
  if (!web_contents()->GetCapturerCount()) {
    GetVideoWakeLock()->CancelWakeLock();
    has_video_wake_lock_for_testing_ = false;
  }
}

void MediaWebContentsObserver::RequestPersistentVideo(bool value) {
  if (!fullscreen_player_)
    return;

  // The message is sent to the renderer even though the video is already the
  // fullscreen element itself. It will eventually be handled by Blink.
  Send(new MediaPlayerDelegateMsg_BecamePersistentVideo(
      fullscreen_player_->first->GetRoutingID(), fullscreen_player_->second,
      value));
}

void MediaWebContentsObserver::OnMediaDestroyed(
    RenderFrameHost* render_frame_host,
    int delegate_id) {
  OnMediaPaused(render_frame_host, delegate_id, true);
}

void MediaWebContentsObserver::OnMediaPaused(RenderFrameHost* render_frame_host,
                                             int delegate_id,
                                             bool reached_end_of_stream) {
  const MediaPlayerId player_id(render_frame_host, delegate_id);
  const bool removed_audio =
      RemoveMediaPlayerEntry(player_id, &active_audio_players_);
  const bool removed_video =
      RemoveMediaPlayerEntry(player_id, &active_video_players_);
  MaybeCancelVideoLock();

  if (removed_audio || removed_video) {
    // Notify observers the player has been "paused".
    web_contents_impl()->MediaStoppedPlaying(
        WebContentsObserver::MediaPlayerInfo(removed_video, removed_audio),
        player_id);
  }

  if (reached_end_of_stream)
    session_controllers_manager_.OnEnd(player_id);
  else
    session_controllers_manager_.OnPause(player_id);
}

void MediaWebContentsObserver::OnMediaPlaying(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool has_video,
    bool has_audio,
    bool is_remote,
    media::MediaContentType media_content_type) {
  // Ignore the videos playing remotely and don't hold the wake lock for the
  // screen. TODO(dalecurtis): Is this correct? It means observers will not
  // receive play and pause messages.
  if (is_remote)
    return;

  const MediaPlayerId id(render_frame_host, delegate_id);
  if (has_audio)
    AddMediaPlayerEntry(id, &active_audio_players_);

  if (has_video) {
    AddMediaPlayerEntry(id, &active_video_players_);

    // If we're not hidden and have just created a player, create a wakelock.
    if (!web_contents_impl()->IsHidden())
      LockVideo();
  }

  if (!session_controllers_manager_.RequestPlay(
          id, has_audio, is_remote, media_content_type)) {
    return;
  }

  // Notify observers of the new player.
  DCHECK(has_audio || has_video);
  web_contents_impl()->MediaStartedPlaying(
      WebContentsObserver::MediaPlayerInfo(has_video, has_audio), id);
}

void MediaWebContentsObserver::OnMediaEffectivelyFullscreenChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool is_fullscreen) {
  const MediaPlayerId id(render_frame_host, delegate_id);

  if (!is_fullscreen) {
    if (fullscreen_player_ && *fullscreen_player_ == id)
      fullscreen_player_.reset();
    return;
  }

  fullscreen_player_ = id;
}

void MediaWebContentsObserver::OnMediaSizeChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    const gfx::Size& size) {
  const MediaPlayerId id(render_frame_host, delegate_id);
  web_contents_impl()->MediaResized(size, id);
}

void MediaWebContentsObserver::ClearWakeLocks(
    RenderFrameHost* render_frame_host) {
  std::set<MediaPlayerId> video_players;
  RemoveAllMediaPlayerEntries(render_frame_host, &active_video_players_,
                              &video_players);
  std::set<MediaPlayerId> audio_players;
  RemoveAllMediaPlayerEntries(render_frame_host, &active_audio_players_,
                              &audio_players);

  std::set<MediaPlayerId> removed_players;
  std::set_union(video_players.begin(), video_players.end(),
                 audio_players.begin(), audio_players.end(),
                 std::inserter(removed_players, removed_players.end()));

  MaybeCancelVideoLock();

  // Notify all observers the player has been "paused".
  for (const auto& id : removed_players) {
    auto it = video_players.find(id);
    bool was_video = (it != video_players.end());
    bool was_audio = (audio_players.find(id) != audio_players.end());
    web_contents_impl()->MediaStoppedPlaying(
        WebContentsObserver::MediaPlayerInfo(was_video, was_audio), id);
  }
}

device::mojom::WakeLock* MediaWebContentsObserver::GetAudioWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!audio_wake_lock_) {
    device::mojom::WakeLockRequest request =
        mojo::MakeRequest(&audio_wake_lock_);
    device::mojom::WakeLockContext* wake_lock_context =
        web_contents()->GetWakeLockContext();
    if (wake_lock_context) {
      wake_lock_context->GetWakeLock(
          device::mojom::WakeLockType::PreventAppSuspension,
          device::mojom::WakeLockReason::ReasonAudioPlayback, "Playing audio",
          std::move(request));
    }
  }
  return audio_wake_lock_.get();
}

device::mojom::WakeLock* MediaWebContentsObserver::GetVideoWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!video_wake_lock_) {
    device::mojom::WakeLockRequest request =
        mojo::MakeRequest(&video_wake_lock_);
    device::mojom::WakeLockContext* wake_lock_context =
        web_contents()->GetWakeLockContext();
    if (wake_lock_context) {
      wake_lock_context->GetWakeLock(
          device::mojom::WakeLockType::PreventDisplaySleep,
          device::mojom::WakeLockReason::ReasonVideoPlayback, "Playing video",
          std::move(request));
    }
  }
  return video_wake_lock_.get();
}

void MediaWebContentsObserver::LockAudio() {
  GetAudioWakeLock()->RequestWakeLock();
  has_audio_wake_lock_for_testing_ = true;
}

void MediaWebContentsObserver::CancelAudioLock() {
  GetAudioWakeLock()->CancelWakeLock();
  has_audio_wake_lock_for_testing_ = false;
}

void MediaWebContentsObserver::LockVideo() {
  DCHECK(!active_video_players_.empty());
  GetVideoWakeLock()->RequestWakeLock();
  has_video_wake_lock_for_testing_ = true;
}

void MediaWebContentsObserver::CancelVideoLock() {
  GetVideoWakeLock()->CancelWakeLock();
  has_video_wake_lock_for_testing_ = false;
}

void MediaWebContentsObserver::MaybeCancelVideoLock() {
  // If there are no more video players, cancel the video wake lock.
  if (active_video_players_.empty())
    CancelVideoLock();
}

void MediaWebContentsObserver::OnMediaMutedStatusChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool muted) {
  const MediaPlayerId id(render_frame_host, delegate_id);
  web_contents_impl()->MediaMutedStatusChanged(id, muted);
}

void MediaWebContentsObserver::AddMediaPlayerEntry(
    const MediaPlayerId& id,
    ActiveMediaPlayerMap* player_map) {
  (*player_map)[id.first].insert(id.second);
}

bool MediaWebContentsObserver::RemoveMediaPlayerEntry(
    const MediaPlayerId& id,
    ActiveMediaPlayerMap* player_map) {
  auto it = player_map->find(id.first);
  if (it == player_map->end())
    return false;

  // Remove the player.
  bool did_remove = it->second.erase(id.second) == 1;
  if (!did_remove)
    return false;

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

WebContentsImpl* MediaWebContentsObserver::web_contents_impl() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

}  // namespace content
