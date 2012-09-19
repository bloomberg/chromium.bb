// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_proxy_impl_android.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/common/view_messages.h"
#include "webkit/media/android/webmediaplayer_impl_android.h"
#include "webkit/media/android/webmediaplayer_manager_android.h"

namespace content {

WebMediaPlayerProxyImplAndroid::WebMediaPlayerProxyImplAndroid(
    content::RenderView* render_view,
    webkit_media::WebMediaPlayerManagerAndroid* manager)
    : content::RenderViewObserver(render_view),
      manager_(manager) {
}

WebMediaPlayerProxyImplAndroid::~WebMediaPlayerProxyImplAndroid() {
  Send(new ViewHostMsg_DestroyAllMediaPlayers(routing_id()));
}

bool WebMediaPlayerProxyImplAndroid::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebMediaPlayerProxyImplAndroid, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPrepared, OnMediaPrepared)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPlaybackCompleted,
                        OnMediaPlaybackCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaBufferingUpdate, OnMediaBufferingUpdate)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaSeekCompleted, OnMediaSeekCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaError, OnMediaError)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaVideoSizeChanged, OnVideoSizeChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaTimeUpdate, OnTimeUpdate)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPlayerReleased, OnMediaPlayerReleased)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebMediaPlayerProxyImplAndroid::Initialize(
    int player_id, const std::string& url,
    const std::string& first_party_for_cookies) {
  Send(new ViewHostMsg_MediaPlayerInitialize(
      routing_id(), player_id, url, first_party_for_cookies));
}

void WebMediaPlayerProxyImplAndroid::Start(int player_id) {
  Send(new ViewHostMsg_MediaPlayerStart(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::Pause(int player_id) {
  Send(new ViewHostMsg_MediaPlayerPause(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::Seek(int player_id, base::TimeDelta time) {
  Send(new ViewHostMsg_MediaPlayerSeek(routing_id(), player_id, time));
}

void WebMediaPlayerProxyImplAndroid::ReleaseResources(int player_id) {
  Send(new ViewHostMsg_MediaPlayerRelease(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::DestroyPlayer(int player_id) {
  Send(new ViewHostMsg_DestroyMediaPlayer(routing_id(), player_id));
}

void WebMediaPlayerProxyImplAndroid::OnMediaPrepared(
    int player_id,
    base::TimeDelta duration) {
  webkit_media::WebMediaPlayerImplAndroid* player =
      GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaPrepared(duration);
}

void WebMediaPlayerProxyImplAndroid::OnMediaPlaybackCompleted(
    int player_id) {
  webkit_media::WebMediaPlayerImplAndroid* player =
      GetWebMediaPlayer(player_id);
  if (player)
    player->OnPlaybackComplete();
}

void WebMediaPlayerProxyImplAndroid::OnMediaBufferingUpdate(
    int player_id, int percent) {
  webkit_media::WebMediaPlayerImplAndroid* player =
      GetWebMediaPlayer(player_id);
  if (player)
    player->OnBufferingUpdate(percent);
}

void WebMediaPlayerProxyImplAndroid::OnMediaSeekCompleted(
    int player_id, base::TimeDelta current_time) {
  webkit_media::WebMediaPlayerImplAndroid* player =
      GetWebMediaPlayer(player_id);
  if (player)
    player->OnSeekComplete(current_time);
}

void WebMediaPlayerProxyImplAndroid::OnMediaError(
    int player_id, int error) {
  webkit_media::WebMediaPlayerImplAndroid* player =
      GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaError(error);
}

void WebMediaPlayerProxyImplAndroid::OnVideoSizeChanged(
    int player_id, int width, int height) {
  webkit_media::WebMediaPlayerImplAndroid* player =
      GetWebMediaPlayer(player_id);
  if (player)
    player->OnVideoSizeChanged(width, height);
}

void WebMediaPlayerProxyImplAndroid::OnTimeUpdate(
    int player_id, base::TimeDelta current_time) {
  webkit_media::WebMediaPlayerImplAndroid* player =
      GetWebMediaPlayer(player_id);
  if (player)
    player->OnTimeUpdate(current_time);
}

void WebMediaPlayerProxyImplAndroid::OnMediaPlayerReleased(
    int player_id) {
  webkit_media::WebMediaPlayerImplAndroid* player =
      GetWebMediaPlayer(player_id);
  if (player)
    player->OnPlayerReleased();
}

webkit_media::WebMediaPlayerImplAndroid*
    WebMediaPlayerProxyImplAndroid::GetWebMediaPlayer(int player_id) {
  return static_cast<webkit_media::WebMediaPlayerImplAndroid*>(
      manager_->GetMediaPlayer(player_id));
}

}  // namespace content
