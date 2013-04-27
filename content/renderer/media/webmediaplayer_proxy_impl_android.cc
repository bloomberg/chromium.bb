// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_proxy_impl_android.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/common/media/media_player_messages.h"
#include "webkit/media/android/webmediaplayer_android.h"
#include "webkit/media/android/webmediaplayer_manager_android.h"

namespace content {

WebMediaPlayerProxyImplAndroid::WebMediaPlayerProxyImplAndroid(
    RenderView* render_view,
    webkit_media::WebMediaPlayerManagerAndroid* manager)
    : RenderViewObserver(render_view),
      manager_(manager) {
}

WebMediaPlayerProxyImplAndroid::~WebMediaPlayerProxyImplAndroid() {
  Send(new MediaPlayerHostMsg_DestroyAllMediaPlayers(routing_id()));
}

bool WebMediaPlayerProxyImplAndroid::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebMediaPlayerProxyImplAndroid, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaMetadataChanged,
                        OnMediaMetadataChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaPlaybackCompleted,
                        OnMediaPlaybackCompleted)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaBufferingUpdate,
                        OnMediaBufferingUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaSeekCompleted, OnMediaSeekCompleted)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaError, OnMediaError)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaVideoSizeChanged,
                        OnVideoSizeChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaTimeUpdate, OnTimeUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaPlayerReleased,
                        OnMediaPlayerReleased)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidEnterFullscreen, OnDidEnterFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidExitFullscreen, OnDidExitFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidMediaPlayerPlay, OnPlayerPlay)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidMediaPlayerPause, OnPlayerPause)
#if defined(GOOGLE_TV)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_ReadFromDemuxer, OnReadFromDemuxer)
#endif
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebMediaPlayerProxyImplAndroid::Initialize(
    int player_id, const GURL& url,
    bool is_media_source,
    const GURL& first_party_for_cookies) {
  Send(new MediaPlayerHostMsg_MediaPlayerInitialize(
      routing_id(), player_id, url, is_media_source, first_party_for_cookies));
}

void WebMediaPlayerProxyImplAndroid::Start(int player_id) {
  Send(new MediaPlayerHostMsg_MediaPlayerStart(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::Pause(int player_id) {
  Send(new MediaPlayerHostMsg_MediaPlayerPause(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::Seek(int player_id, base::TimeDelta time) {
  Send(new MediaPlayerHostMsg_MediaPlayerSeek(routing_id(), player_id, time));
}

void WebMediaPlayerProxyImplAndroid::ReleaseResources(int player_id) {
  Send(new MediaPlayerHostMsg_MediaPlayerRelease(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::DestroyPlayer(int player_id) {
  Send(new MediaPlayerHostMsg_DestroyMediaPlayer(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::OnMediaMetadataChanged(
    int player_id,
    base::TimeDelta duration,
    int width,
    int height,
    bool success) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaMetadataChanged(duration, width, height, success);
}

void WebMediaPlayerProxyImplAndroid::OnMediaPlaybackCompleted(
    int player_id) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnPlaybackComplete();
}

void WebMediaPlayerProxyImplAndroid::OnMediaBufferingUpdate(
    int player_id, int percent) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnBufferingUpdate(percent);
}

void WebMediaPlayerProxyImplAndroid::OnMediaSeekCompleted(
    int player_id, base::TimeDelta current_time) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnSeekComplete(current_time);
}

void WebMediaPlayerProxyImplAndroid::OnMediaError(
    int player_id, int error) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaError(error);
}

void WebMediaPlayerProxyImplAndroid::OnVideoSizeChanged(
    int player_id, int width, int height) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnVideoSizeChanged(width, height);
}

void WebMediaPlayerProxyImplAndroid::OnTimeUpdate(
    int player_id, base::TimeDelta current_time) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnTimeUpdate(current_time);
}

void WebMediaPlayerProxyImplAndroid::OnMediaPlayerReleased(
    int player_id) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnPlayerReleased();
}

void WebMediaPlayerProxyImplAndroid::OnDidEnterFullscreen(
    int player_id) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnDidEnterFullscreen();
}

void WebMediaPlayerProxyImplAndroid::OnDidExitFullscreen(
    int player_id) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnDidExitFullscreen();
}

void WebMediaPlayerProxyImplAndroid::OnPlayerPlay(int player_id) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaPlayerPlay();
}

void WebMediaPlayerProxyImplAndroid::OnPlayerPause(int player_id) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaPlayerPause();
}

void WebMediaPlayerProxyImplAndroid::EnterFullscreen(int player_id) {
  Send(new MediaPlayerHostMsg_EnterFullscreen(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::ExitFullscreen(int player_id) {
  Send(new MediaPlayerHostMsg_ExitFullscreen(routing_id(), player_id));
}

#if defined(GOOGLE_TV)
void WebMediaPlayerProxyImplAndroid::RequestExternalSurface(int player_id) {
  Send(new MediaPlayerHostMsg_RequestExternalSurface(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::DidCommitCompositorFrame() {
  std::map<int, gfx::RectF> geometry_change;
  manager_->RetrieveGeometryChanges(&geometry_change);
  for (std::map<int, gfx::RectF>::iterator it = geometry_change.begin();
       it != geometry_change.end();
       ++it) {
    Send(new MediaPlayerHostMsg_NotifyGeometryChange(routing_id(),
                                                     it->first,
                                                     it->second));
  }
}

void WebMediaPlayerProxyImplAndroid::DemuxerReady(
    int player_id,
    const media::MediaPlayerHostMsg_DemuxerReady_Params& params) {
  Send(new MediaPlayerHostMsg_DemuxerReady(routing_id(), player_id, params));
}

void WebMediaPlayerProxyImplAndroid::ReadFromDemuxerAck(
    int player_id,
    const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) {
  Send(new MediaPlayerHostMsg_ReadFromDemuxerAck(
      routing_id(), player_id, params));

}
void WebMediaPlayerProxyImplAndroid::OnReadFromDemuxer(
    int player_id, media::DemuxerStream::Type type, bool seek_done) {
  webkit_media::WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnReadFromDemuxer(type, seek_done);
}
#endif

webkit_media::WebMediaPlayerAndroid*
    WebMediaPlayerProxyImplAndroid::GetWebMediaPlayer(int player_id) {
  return static_cast<webkit_media::WebMediaPlayerAndroid*>(
      manager_->GetMediaPlayer(player_id));
}

}  // namespace content
