// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediaplayer_proxy_android.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/webmediaplayer_android.h"

namespace content {

WebMediaPlayerProxyAndroid::WebMediaPlayerProxyAndroid(
    RenderView* render_view,
    RendererMediaPlayerManager* manager)
    : RenderViewObserver(render_view), manager_(manager) {}

WebMediaPlayerProxyAndroid::~WebMediaPlayerProxyAndroid() {
  Send(new MediaPlayerHostMsg_DestroyAllMediaPlayers(routing_id()));
}

bool WebMediaPlayerProxyAndroid::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebMediaPlayerProxyAndroid, msg)
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
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_ReadFromDemuxer, OnReadFromDemuxer)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaSeekRequest, OnMediaSeekRequest)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaConfigRequest, OnMediaConfigRequest)
    IPC_MESSAGE_HANDLER(MediaKeysMsg_KeyAdded, OnKeyAdded)
    IPC_MESSAGE_HANDLER(MediaKeysMsg_KeyError, OnKeyError)
    IPC_MESSAGE_HANDLER(MediaKeysMsg_KeyMessage, OnKeyMessage)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebMediaPlayerProxyAndroid::Initialize(
    int player_id,
    const GURL& url,
    media::MediaPlayerAndroid::SourceType source_type,
    const GURL& first_party_for_cookies) {
  Send(new MediaPlayerHostMsg_Initialize(
      routing_id(), player_id, url, source_type, first_party_for_cookies));
}

void WebMediaPlayerProxyAndroid::Start(int player_id) {
  Send(new MediaPlayerHostMsg_Start(routing_id(), player_id));
}

void WebMediaPlayerProxyAndroid::Pause(int player_id) {
  Send(new MediaPlayerHostMsg_Pause(routing_id(), player_id));
}

void WebMediaPlayerProxyAndroid::Seek(int player_id, base::TimeDelta time) {
  Send(new MediaPlayerHostMsg_Seek(routing_id(), player_id, time));
}

void WebMediaPlayerProxyAndroid::SetVolume(int player_id, double volume) {
  Send(new MediaPlayerHostMsg_SetVolume(routing_id(), player_id, volume));
}

void WebMediaPlayerProxyAndroid::ReleaseResources(int player_id) {
  Send(new MediaPlayerHostMsg_Release(routing_id(), player_id));
}

void WebMediaPlayerProxyAndroid::DestroyPlayer(int player_id) {
  Send(new MediaPlayerHostMsg_DestroyMediaPlayer(routing_id(), player_id));
}

void WebMediaPlayerProxyAndroid::OnMediaMetadataChanged(
    int player_id,
    base::TimeDelta duration,
    int width,
    int height,
    bool success) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaMetadataChanged(duration, width, height, success);
}

void WebMediaPlayerProxyAndroid::OnMediaPlaybackCompleted(int player_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnPlaybackComplete();
}

void WebMediaPlayerProxyAndroid::OnMediaBufferingUpdate(int player_id,
                                                        int percent) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnBufferingUpdate(percent);
}

void WebMediaPlayerProxyAndroid::OnMediaSeekCompleted(
    int player_id,
    base::TimeDelta current_time) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnSeekComplete(current_time);
}

void WebMediaPlayerProxyAndroid::OnMediaError(int player_id, int error) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaError(error);
}

void WebMediaPlayerProxyAndroid::OnVideoSizeChanged(int player_id,
                                                    int width,
                                                    int height) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnVideoSizeChanged(width, height);
}

void WebMediaPlayerProxyAndroid::OnTimeUpdate(int player_id,
                                              base::TimeDelta current_time) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnTimeUpdate(current_time);
}

void WebMediaPlayerProxyAndroid::OnMediaPlayerReleased(int player_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnPlayerReleased();
}

void WebMediaPlayerProxyAndroid::OnDidEnterFullscreen(int player_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnDidEnterFullscreen();
}

void WebMediaPlayerProxyAndroid::OnDidExitFullscreen(int player_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnDidExitFullscreen();
}

void WebMediaPlayerProxyAndroid::OnPlayerPlay(int player_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaPlayerPlay();
}

void WebMediaPlayerProxyAndroid::OnPlayerPause(int player_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaPlayerPause();
}

void WebMediaPlayerProxyAndroid::EnterFullscreen(int player_id) {
  Send(new MediaPlayerHostMsg_EnterFullscreen(routing_id(), player_id));
}

void WebMediaPlayerProxyAndroid::ExitFullscreen(int player_id) {
  Send(new MediaPlayerHostMsg_ExitFullscreen(routing_id(), player_id));
}

void WebMediaPlayerProxyAndroid::ReadFromDemuxerAck(
    int player_id,
    const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) {
  Send(new MediaPlayerHostMsg_ReadFromDemuxerAck(
      routing_id(), player_id, params));
}

void WebMediaPlayerProxyAndroid::SeekRequestAck(int player_id,
                                                unsigned seek_request_id) {
  Send(new MediaPlayerHostMsg_MediaSeekRequestAck(
      routing_id(), player_id, seek_request_id));
}

#if defined(GOOGLE_TV)
void WebMediaPlayerProxyAndroid::RequestExternalSurface(
    int player_id,
    const gfx::RectF& geometry) {
  Send(new MediaPlayerHostMsg_NotifyExternalSurface(
      routing_id(), player_id, true, geometry));
}

void WebMediaPlayerProxyAndroid::DidCommitCompositorFrame() {
  std::map<int, gfx::RectF> geometry_change;
  manager_->RetrieveGeometryChanges(&geometry_change);
  for (std::map<int, gfx::RectF>::iterator it = geometry_change.begin();
       it != geometry_change.end();
       ++it) {
    Send(new MediaPlayerHostMsg_NotifyExternalSurface(
        routing_id(), it->first, false, it->second));
  }
}
#endif

void WebMediaPlayerProxyAndroid::OnReadFromDemuxer(
    int player_id,
    media::DemuxerStream::Type type) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnReadFromDemuxer(type);
}

void WebMediaPlayerProxyAndroid::DemuxerReady(
    int player_id,
    const media::MediaPlayerHostMsg_DemuxerReady_Params& params) {
  Send(new MediaPlayerHostMsg_DemuxerReady(routing_id(), player_id, params));
}

void WebMediaPlayerProxyAndroid::DurationChanged(
    int player_id,
    const base::TimeDelta& duration) {
  Send(new MediaPlayerHostMsg_DurationChanged(
      routing_id(), player_id, duration));
}

void WebMediaPlayerProxyAndroid::InitializeCDM(int media_keys_id,
                                               const std::vector<uint8>& uuid) {
  Send(new MediaKeysHostMsg_InitializeCDM(routing_id(), media_keys_id, uuid));
}

void WebMediaPlayerProxyAndroid::GenerateKeyRequest(
    int media_keys_id,
    const std::string& type,
    const std::vector<uint8>& init_data) {
  Send(new MediaKeysHostMsg_GenerateKeyRequest(
      routing_id(), media_keys_id, type, init_data));
}

void WebMediaPlayerProxyAndroid::AddKey(int media_keys_id,
                                        const std::vector<uint8>& key,
                                        const std::vector<uint8>& init_data,
                                        const std::string& session_id) {
  Send(new MediaKeysHostMsg_AddKey(
      routing_id(), media_keys_id, key, init_data, session_id));
}

void WebMediaPlayerProxyAndroid::CancelKeyRequest(
    int media_keys_id,
    const std::string& session_id) {
  Send(new MediaKeysHostMsg_CancelKeyRequest(
      routing_id(), media_keys_id, session_id));
}

WebMediaPlayerAndroid* WebMediaPlayerProxyAndroid::GetWebMediaPlayer(
    int player_id) {
  return static_cast<WebMediaPlayerAndroid*>(
      manager_->GetMediaPlayer(player_id));
}

void WebMediaPlayerProxyAndroid::OnMediaSeekRequest(
    int player_id,
    base::TimeDelta time_to_seek,
    unsigned seek_request_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaSeekRequest(time_to_seek, seek_request_id);
}

void WebMediaPlayerProxyAndroid::OnMediaConfigRequest(int player_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(player_id);
  if (player)
    player->OnMediaConfigRequest();
}

void WebMediaPlayerProxyAndroid::OnKeyAdded(int media_keys_id,
                                            const std::string& session_id) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(media_keys_id);
  if (player)
    player->OnKeyAdded(session_id);
}

void WebMediaPlayerProxyAndroid::OnKeyError(
    int media_keys_id,
    const std::string& session_id,
    media::MediaKeys::KeyError error_code,
    int system_code) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(media_keys_id);
  if (player)
    player->OnKeyError(session_id, error_code, system_code);
}

void WebMediaPlayerProxyAndroid::OnKeyMessage(
    int media_keys_id,
    const std::string& session_id,
    const std::vector<uint8>& message,
    const std::string& destination_url) {
  WebMediaPlayerAndroid* player = GetWebMediaPlayer(media_keys_id);
  if (player)
    player->OnKeyMessage(session_id, message, destination_url);
}

}  // namespace content
