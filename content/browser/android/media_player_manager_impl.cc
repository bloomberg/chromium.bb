// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/media_player_manager_impl.h"

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

MediaPlayerManagerImpl::MediaPlayerManagerImpl(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      video_view_(this),
      fullscreen_player_id_(-1),
      web_contents_(WebContents::FromRenderViewHost(render_view_host)) {
}

MediaPlayerManagerImpl::~MediaPlayerManagerImpl() {}

bool MediaPlayerManagerImpl::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaPlayerManagerImpl, msg)
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
#if defined(GOOGLE_TV)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_RequestExternalSurface,
                        OnRequestExternalSurface)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_NotifyGeometryChange,
                        OnNotifyGeometryChange)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DemuxerReady,
                        OnDemuxerReady)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_ReadFromDemuxerAck,
                        OnReadFromDemuxerAck)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaPlayerManagerImpl::FullscreenPlayerPlay() {
  MediaPlayerBridge* player = GetFullscreenPlayer();
  if (player) {
    player->Start();
    Send(new MediaPlayerMsg_DidMediaPlayerPlay(
        routing_id(), fullscreen_player_id_));
  }
}

void MediaPlayerManagerImpl::FullscreenPlayerPause() {
  MediaPlayerBridge* player = GetFullscreenPlayer();
  if (player) {
    player->Pause();
    Send(new MediaPlayerMsg_DidMediaPlayerPause(
        routing_id(), fullscreen_player_id_));
  }
}

void MediaPlayerManagerImpl::FullscreenPlayerSeek(int msec) {
  MediaPlayerBridge* player = GetFullscreenPlayer();
  if (player)
    player->SeekTo(base::TimeDelta::FromMilliseconds(msec));
}

void MediaPlayerManagerImpl::ExitFullscreen(bool release_media_player) {
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

void MediaPlayerManagerImpl::SetVideoSurface(jobject surface) {
  MediaPlayerBridge* player = GetFullscreenPlayer();
  if (player) {
    player->SetVideoSurface(surface);
    Send(new MediaPlayerMsg_DidEnterFullscreen(
        routing_id(), player->player_id()));
  }
}

void MediaPlayerManagerImpl::OnInitialize(
    int player_id, const GURL& url,
    bool is_media_source,
    const GURL& first_party_for_cookies) {
  for (ScopedVector<MediaPlayerBridge>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id) {
      players_.erase(it);
      break;
    }
  }

  RenderProcessHost* host = render_view_host()->GetProcess();
  players_.push_back(media::MediaPlayerBridge::Create(
      player_id, url, is_media_source, first_party_for_cookies,
      host->GetBrowserContext()->IsOffTheRecord(), this,
#if defined(GOOGLE_TV)
      base::Bind(&MediaPlayerManagerImpl::OnReadFromDemuxer,
                 base::Unretained(this)),
#endif
      base::Bind(&MediaPlayerManagerImpl::OnError, base::Unretained(this)),
      base::Bind(&MediaPlayerManagerImpl::OnVideoSizeChanged,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerImpl::OnBufferingUpdate,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerImpl::OnMediaMetadataChanged,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerImpl::OnPlaybackComplete,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerImpl::OnSeekComplete,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerImpl::OnTimeUpdate,
                 base::Unretained(this)),
      base::Bind(&MediaPlayerManagerImpl::OnMediaInterrupted,
                 base::Unretained(this))));
}

media::MediaResourceGetter* MediaPlayerManagerImpl::GetMediaResourceGetter() {
  if (!media_resource_getter_.get()) {
    RenderProcessHost* host = render_view_host()->GetProcess();
    BrowserContext* context = host->GetBrowserContext();
    StoragePartition* partition = host->GetStoragePartition();
    fileapi::FileSystemContext* file_system_context =
        partition ? partition->GetFileSystemContext() : NULL;
    media_resource_getter_.reset(new MediaResourceGetterImpl(
        context, file_system_context, host->GetID(), routing_id()));
  }
  return media_resource_getter_.get();
}

void MediaPlayerManagerImpl::OnStart(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->Start();
}

void MediaPlayerManagerImpl::OnSeek(int player_id, base::TimeDelta time) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->SeekTo(time);
}

void MediaPlayerManagerImpl::OnPause(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->Pause();
}

void MediaPlayerManagerImpl::OnEnterFullscreen(int player_id) {
  DCHECK_EQ(fullscreen_player_id_, -1);

  fullscreen_player_id_ = player_id;
  video_view_.CreateContentVideoView();
}

void MediaPlayerManagerImpl::OnExitFullscreen(int player_id) {
  if (fullscreen_player_id_ == player_id) {
    MediaPlayerBridge* player = GetPlayer(player_id);
    if (player)
      player->SetVideoSurface(NULL);
    video_view_.DestroyContentVideoView();
    fullscreen_player_id_ = -1;
  }
}

void MediaPlayerManagerImpl::OnReleaseResources(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  // Don't release the fullscreen player when tab visibility changes,
  // it will be released when user hit the back/home button or when
  // OnDestroyPlayer is called.
  if (player && player_id != fullscreen_player_id_)
    player->Release();
}

void MediaPlayerManagerImpl::OnDestroyPlayer(int player_id) {
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

void MediaPlayerManagerImpl::DestroyAllMediaPlayers() {
  players_.clear();
  if (fullscreen_player_id_ != -1) {
    video_view_.DestroyContentVideoView();
    fullscreen_player_id_ = -1;
  }
}

#if defined(GOOGLE_TV)
void MediaPlayerManagerImpl::AttachExternalVideoSurface(int player_id,
                                                           jobject surface) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->SetVideoSurface(surface);
}

void MediaPlayerManagerImpl::DetachExternalVideoSurface(int player_id) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->SetVideoSurface(NULL);
}

void MediaPlayerManagerImpl::OnRequestExternalSurface(int player_id) {
  if (!web_contents_)
    return;

  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  if (view)
    view->RequestExternalVideoSurface(player_id);
}

void MediaPlayerManagerImpl::OnNotifyGeometryChange(int player_id,
                                                       const gfx::RectF& rect) {
  if (!web_contents_)
    return;

  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  if (view)
    view->NotifyGeometryChange(player_id, rect);
}

void MediaPlayerManagerImpl::OnDemuxerReady(
    int player_id,
    const media::MediaPlayerHostMsg_DemuxerReady_Params& params) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->DemuxerReady(params);
}

void MediaPlayerManagerImpl::OnReadFromDemuxerAck(
    int player_id,
    const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) {
  MediaPlayerBridge* player = GetPlayer(player_id);
  if (player)
    player->ReadFromDemuxerAck(params);
}
#endif

MediaPlayerBridge* MediaPlayerManagerImpl::GetPlayer(int player_id) {
  for (ScopedVector<MediaPlayerBridge>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id)
      return *it;
  }
  return NULL;
}

MediaPlayerBridge* MediaPlayerManagerImpl::GetFullscreenPlayer() {
  return GetPlayer(fullscreen_player_id_);
}

void MediaPlayerManagerImpl::OnMediaMetadataChanged(
    int player_id, base::TimeDelta duration, int width, int height,
    bool success) {
  Send(new MediaPlayerMsg_MediaMetadataChanged(
      routing_id(), player_id, duration, width, height, success));
  if (fullscreen_player_id_ != -1)
    video_view_.UpdateMediaMetadata();
}

void MediaPlayerManagerImpl::OnPlaybackComplete(int player_id) {
  Send(new MediaPlayerMsg_MediaPlaybackCompleted(routing_id(), player_id));
  if (fullscreen_player_id_ != -1)
    video_view_.OnPlaybackComplete();
}

void MediaPlayerManagerImpl::OnMediaInterrupted(int player_id) {
  // Tell WebKit that the audio should be paused, then release all resources
  Send(new MediaPlayerMsg_DidMediaPlayerPause(routing_id(), player_id));
  OnReleaseResources(player_id);
}

void MediaPlayerManagerImpl::OnBufferingUpdate(
    int player_id, int percentage) {
  Send(new MediaPlayerMsg_MediaBufferingUpdate(
      routing_id(), player_id, percentage));
  if (fullscreen_player_id_ != -1)
    video_view_.OnBufferingUpdate(percentage);
}

void MediaPlayerManagerImpl::OnSeekComplete(int player_id,
                                               base::TimeDelta current_time) {
  Send(new MediaPlayerMsg_MediaSeekCompleted(
      routing_id(), player_id, current_time));
}

void MediaPlayerManagerImpl::OnError(int player_id, int error) {
  Send(new MediaPlayerMsg_MediaError(routing_id(), player_id, error));
  if (fullscreen_player_id_ != -1)
    video_view_.OnMediaPlayerError(error);
}

void MediaPlayerManagerImpl::OnVideoSizeChanged(
    int player_id, int width, int height) {
  Send(new MediaPlayerMsg_MediaVideoSizeChanged(routing_id(), player_id,
      width, height));
  if (fullscreen_player_id_ != -1)
    video_view_.OnVideoSizeChanged(width, height);
}

void MediaPlayerManagerImpl::OnTimeUpdate(int player_id,
                                             base::TimeDelta current_time) {
  Send(new MediaPlayerMsg_MediaTimeUpdate(
      routing_id(), player_id, current_time));
}

#if defined(GOOGLE_TV)
void MediaPlayerManagerImpl::OnReadFromDemuxer(
    int player_id, media::DemuxerStream::Type type, bool seek_done) {
  Send(new MediaPlayerMsg_ReadFromDemuxer(
      routing_id(), player_id, type, seek_done));
}
#endif

void MediaPlayerManagerImpl::RequestMediaResources(
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

void MediaPlayerManagerImpl::ReleaseMediaResources(
    MediaPlayerBridge* player) {
  // Nothing needs to be done.
}

}  // namespace content
