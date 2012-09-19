// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/media_player_manager_android.h"

#include "base/bind.h"
#include "content/browser/android/cookie_getter_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

using media::MediaPlayerBridge;

// Threshold on the number of media players per renderer before we start
// attempting to release inactive media players.
static const int kMediaPlayerThreshold = 1;

namespace content {

MediaPlayerManagerAndroid::MediaPlayerManagerAndroid(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host) {
}

MediaPlayerManagerAndroid::~MediaPlayerManagerAndroid() {}

bool MediaPlayerManagerAndroid::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaPlayerManagerAndroid, msg)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaPlayerInitialize,
                        OnInitialize)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaPlayerStart,
                        OnStart)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaPlayerSeek,
                        OnSeek)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaPlayerPause,
                        OnPause)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaPlayerRelease,
                        OnReleaseResources)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DestroyMediaPlayer,
                        OnDestroyPlayer)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DestroyAllMediaPlayers,
                        DestroyAllMediaPlayers)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaPlayerManagerAndroid::OnInitialize(
    int player_id, const std::string& url,
    const std::string& first_party_for_cookies) {
  for (ScopedVector<MediaPlayerBridge>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id) {
      players_.erase(it);
      break;
    }
  }

  RenderProcessHost* host = render_view_host()->GetProcess();
  BrowserContext* context = host->GetBrowserContext();
  players_.push_back(new MediaPlayerBridge(
      player_id, url, first_party_for_cookies,
      new CookieGetterImpl(context, host->GetID(), routing_id()),
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
                 base::Unretained(this))));

  // Send a MediaPrepared message to webkit so that Load() can finish.
  Send(new ViewMsg_MediaPrepared(routing_id(), player_id,
                                 GetPlayer(player_id)->GetDuration()));
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

void MediaPlayerManagerAndroid::OnReleaseResources(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
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
}

void MediaPlayerManagerAndroid::DestroyAllMediaPlayers() {
  players_.clear();
}

MediaPlayerBridge* MediaPlayerManagerAndroid::GetPlayer(int player_id) {
  for (ScopedVector<MediaPlayerBridge>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id)
      return *it;
  }
  return NULL;
}

void MediaPlayerManagerAndroid::OnPrepared(int player_id,
                                           base::TimeDelta duration) {
  Send(new ViewMsg_MediaPrepared(routing_id(), player_id, duration));
}

void MediaPlayerManagerAndroid::OnPlaybackComplete(int player_id) {
  Send(new ViewMsg_MediaPlaybackCompleted(routing_id(), player_id));
}

void MediaPlayerManagerAndroid::OnBufferingUpdate(
    int player_id, int percentage) {
  Send(new ViewMsg_MediaBufferingUpdate(routing_id(), player_id, percentage));
}

void MediaPlayerManagerAndroid::OnSeekComplete(int player_id,
                                               base::TimeDelta current_time) {
  Send(new ViewMsg_MediaSeekCompleted(routing_id(), player_id, current_time));
}

void MediaPlayerManagerAndroid::OnError(int player_id, int error) {
  Send(new ViewMsg_MediaError(routing_id(), player_id, error));
}

void MediaPlayerManagerAndroid::OnVideoSizeChanged(
    int player_id, int width, int height) {
  Send(new ViewMsg_MediaVideoSizeChanged(routing_id(), player_id,
      width, height));
}

void MediaPlayerManagerAndroid::OnTimeUpdate(int player_id,
                                             base::TimeDelta current_time) {
  Send(new ViewMsg_MediaTimeUpdate(routing_id(), player_id, current_time));
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
    if ((*it)->prepared() && !(*it)->IsPlaying()) {
      (*it)->Release();
      Send(new ViewMsg_MediaPlayerReleased(routing_id(), (*it)->player_id()));
    }
  }
}

void MediaPlayerManagerAndroid::ReleaseMediaResources(
    MediaPlayerBridge* player) {
  // Nothing needs to be done.
}

}  // namespace content
