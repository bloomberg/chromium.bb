// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/media_player_manager_android.h"

#include "base/bind.h"
#include "content/browser/android/media_resource_getter_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/media/media_player_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"

using media::MediaPlayerBridge;

// Threshold on the number of media players per renderer before we start
// attempting to release inactive media players.
static const int kMediaPlayerThreshold = 1;

namespace content {

MediaPlayerManagerAndroid::MediaPlayerManagerAndroid(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      ALLOW_THIS_IN_INITIALIZER_LIST(video_view_(this)),
      fullscreen_player_id_(-1),
      web_contents_(WebContents::FromRenderViewHost(render_view_host)) {
}

MediaPlayerManagerAndroid::~MediaPlayerManagerAndroid() {}

bool MediaPlayerManagerAndroid::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaPlayerManagerAndroid, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_EnterFullscreen, OnEnterFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_ExitFullscreen, OnExitFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerInitialize, OnInitialize)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerStart, OnStart)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerSeek, OnSeek)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerPause, OnPause)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerRelease,
                        OnReleaseResources)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DestroyMediaPlayer, OnDestroyPlayer)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DestroyAllMediaPlayers,
                        DestroyAllMediaPlayers)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_RequestExternalSurface,
                        OnRequestExternalSurface)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaPlayerManagerAndroid::FullscreenPlayerPlay() {
  MediaPlayerBridge* player = GetFullscreenPlayer();
  if (player) {
    player->Start();
    Send(new MediaPlayerMsg_DidMediaPlayerPlay(
        routing_id(), fullscreen_player_id_));
  }
}

void MediaPlayerManagerAndroid::FullscreenPlayerPause() {
  MediaPlayerBridge* player = GetFullscreenPlayer();
  if (player) {
    player->Pause();
    Send(new MediaPlayerMsg_DidMediaPlayerPause(
        routing_id(), fullscreen_player_id_));
  }
}

void MediaPlayerManagerAndroid::FullscreenPlayerSeek(int msec) {
  MediaPlayerBridge* player = GetFullscreenPlayer();
  if (player)
    player->SeekTo(base::TimeDelta::FromMilliseconds(msec));
}

void MediaPlayerManagerAndroid::ExitFullscreen(bool release_media_player) {
  Send(new MediaPlayerMsg_DidExitFullscreen(
      routing_id(), fullscreen_player_id_));
  MediaPlayerBridge* player = GetFullscreenPlayer();
  fullscreen_player_id_ = -1;
  if (!player)
    return;
  if (release_media_player)
    player->Release();
  else
    player->SetVideoSurface(NULL);
}

void MediaPlayerManagerAndroid::SetVideoSurface(jobject surface) {
  MediaPlayerBridge* player = GetFullscreenPlayer();
  if (player) {
    player->SetVideoSurface(surface);
    Send(new MediaPlayerMsg_DidEnterFullscreen(
        routing_id(), player->player_id()));
  }
}

void MediaPlayerManagerAndroid::OnInitialize(
    int player_id, const GURL& url, const GURL& first_party_for_cookies) {
  for (ScopedVector<MediaPlayerBridge>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id) {
      players_.erase(it);
      break;
    }
  }

  RenderProcessHost* host = render_view_host()->GetProcess();
  BrowserContext* context = host->GetBrowserContext();
  StoragePartition* partition = host->GetStoragePartition();
  fileapi::FileSystemContext* file_system_context =
      partition ? partition->GetFileSystemContext() : NULL;
  players_.push_back(new MediaPlayerBridge(
      player_id, url, first_party_for_cookies,
      new MediaResourceGetterImpl(context, file_system_context, host->GetID(),
                                  routing_id()),
      context->IsOffTheRecord(), this,
      base::Bind(&MediaPlayerManagerAndroid::OnError, base::Unretained(this)),
      base::Bind(&MediaPlayerManagerAndroid::OnVideoSizeChanged,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerAndroid::OnBufferingUpdate,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerAndroid::OnPrepared,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerAndroid::OnPlaybackComplete,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerAndroid::OnSeekComplete,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerAndroid::OnTimeUpdate,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerAndroid::OnMediaInterrupted,
                 base::Unretained(this))));

  // Send a MediaPrepared message to webkit so that Load() can finish.
  Send(new MediaPlayerMsg_MediaPrepared(
      routing_id(), player_id, GetPlayer(player_id)->GetDuration()));
}

void MediaPlayerManagerAndroid::OnStart(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->Start();
}

void MediaPlayerManagerAndroid::OnSeek(int player_id, base::TimeDelta time) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->SeekTo(time);
}

void MediaPlayerManagerAndroid::OnPause(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->Pause();
}

void MediaPlayerManagerAndroid::OnEnterFullscreen(int player_id) {
  DCHECK_EQ(fullscreen_player_id_, -1);

  fullscreen_player_id_ = player_id;
  video_view_.CreateContentVideoView();
}

void MediaPlayerManagerAndroid::OnExitFullscreen(int player_id) {
  if (fullscreen_player_id_ == player_id) {
    MediaPlayerBridge* player = GetPlayer(player_id);
    if (player)
      player->SetVideoSurface(NULL);
    video_view_.DestroyContentVideoView();
    fullscreen_player_id_ = -1;
  }
}

void MediaPlayerManagerAndroid::OnReleaseResources(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  // Don't release the fullscreen player when tab visibility changes,
  // it will be released when user hit the back/home button or when
  // OnDestroyPlayer is called.
  if (player && player_id != fullscreen_player_id_)
    player->Release();
}

void MediaPlayerManagerAndroid::OnDestroyPlayer(int player_id) {
  for (ScopedVector<MediaPlayerBridge>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id) {
      players_.erase(it);
      break;
    }
  }
  if (fullscreen_player_id_ == player_id)
    fullscreen_player_id_ = -1;
}

void MediaPlayerManagerAndroid::DestroyAllMediaPlayers() {
  players_.clear();
  if (fullscreen_player_id_ != -1) {
    video_view_.DestroyContentVideoView();
    fullscreen_player_id_ = -1;
  }
}

void MediaPlayerManagerAndroid::AttachExternalVideoSurface(int player_id,
                                                           jobject surface) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->SetVideoSurface(surface);
}

void MediaPlayerManagerAndroid::DetachExternalVideoSurface(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->SetVideoSurface(NULL);
}

void MediaPlayerManagerAndroid::OnRequestExternalSurface(int player_id) {
  if (web_contents_) {
    WebContentsViewAndroid* view =
        static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
    if (view)
      view->RequestExternalVideoSurface(player_id);
  }
}

MediaPlayerBridge* MediaPlayerManagerAndroid::GetPlayer(int player_id) {
  for (ScopedVector<MediaPlayerBridge>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id)
      return *it;
  }
  return NULL;
}

MediaPlayerBridge* MediaPlayerManagerAndroid::GetFullscreenPlayer() {
  return GetPlayer(fullscreen_player_id_);
}

void MediaPlayerManagerAndroid::OnPrepared(int player_id,
                                           base::TimeDelta duration) {
  Send(new MediaPlayerMsg_MediaPrepared(routing_id(), player_id, duration));
  if (fullscreen_player_id_ != -1)
    video_view_.UpdateMediaMetadata();
}

void MediaPlayerManagerAndroid::OnPlaybackComplete(int player_id) {
  Send(new MediaPlayerMsg_MediaPlaybackCompleted(routing_id(), player_id));
  if (fullscreen_player_id_ != -1)
    video_view_.OnPlaybackComplete();
}

void MediaPlayerManagerAndroid::OnMediaInterrupted(int player_id) {
  // Tell WebKit that the audio should be paused, then release all resources
  Send(new MediaPlayerMsg_DidMediaPlayerPause(routing_id(), player_id));
  OnReleaseResources(player_id);
}

void MediaPlayerManagerAndroid::OnBufferingUpdate(
    int player_id, int percentage) {
  Send(new MediaPlayerMsg_MediaBufferingUpdate(
      routing_id(), player_id, percentage));
  if (fullscreen_player_id_ != -1)
    video_view_.OnBufferingUpdate(percentage);
}

void MediaPlayerManagerAndroid::OnSeekComplete(int player_id,
                                               base::TimeDelta current_time) {
  Send(new MediaPlayerMsg_MediaSeekCompleted(
      routing_id(), player_id, current_time));
}

void MediaPlayerManagerAndroid::OnError(int player_id, int error) {
  Send(new MediaPlayerMsg_MediaError(routing_id(), player_id, error));
  if (fullscreen_player_id_ != -1)
    video_view_.OnMediaPlayerError(error);
}

void MediaPlayerManagerAndroid::OnVideoSizeChanged(
    int player_id, int width, int height) {
  Send(new MediaPlayerMsg_MediaVideoSizeChanged(routing_id(), player_id,
      width, height));
  if (fullscreen_player_id_ != -1)
    video_view_.OnVideoSizeChanged(width, height);
}

void MediaPlayerManagerAndroid::OnTimeUpdate(int player_id,
                                             base::TimeDelta current_time) {
  Send(new MediaPlayerMsg_MediaTimeUpdate(
      routing_id(), player_id, current_time));
}

void MediaPlayerManagerAndroid::RequestMediaResources(
    MediaPlayerBridge* player) {
  if (player == NULL)
    return;

  int num_active_player = 0;
  ScopedVector<MediaPlayerBridge>::iterator it;
  for (it = players_.begin(); it != players_.end(); ++it) {
    if (!(*it)->prepared())
      continue;

    // The player is already active, ignore it.
    if ((*it) == player)
      return;
    else
      num_active_player++;
  }

  // Number of active players are less than the threshold, do nothing.
  if (num_active_player < kMediaPlayerThreshold)
    return;

  for (it = players_.begin(); it != players_.end(); ++it) {
    if ((*it)->prepared() && !(*it)->IsPlaying() &&
        fullscreen_player_id_ != (*it)->player_id()) {
      (*it)->Release();
      Send(new MediaPlayerMsg_MediaPlayerReleased(
          routing_id(), (*it)->player_id()));
    }
  }
}

void MediaPlayerManagerAndroid::ReleaseMediaResources(
    MediaPlayerBridge* player) {
  // Nothing needs to be done.
}

}  // namespace content
