// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/renderer_media_player_manager.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/renderer/media/android/proxy_media_keys.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/webmediaplayer_android.h"
#include "ui/gfx/rect_f.h"

namespace content {

RendererMediaPlayerManager::RendererMediaPlayerManager(RenderView* render_view)
    : RenderViewObserver(render_view),
      next_media_player_id_(0),
      fullscreen_frame_(NULL) {}

RendererMediaPlayerManager::~RendererMediaPlayerManager() {
  std::map<int, WebMediaPlayerAndroid*>::iterator player_it;
  for (player_it = media_players_.begin();
      player_it != media_players_.end(); ++player_it) {
    WebMediaPlayerAndroid* player = player_it->second;
    player->Detach();
  }

  Send(new MediaPlayerHostMsg_DestroyAllMediaPlayers(routing_id()));
}

bool RendererMediaPlayerManager::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererMediaPlayerManager, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaMetadataChanged,
                        OnMediaMetadataChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaPlaybackCompleted,
                        OnMediaPlaybackCompleted)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaBufferingUpdate,
                        OnMediaBufferingUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_SeekRequest, OnSeekRequest)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_SeekCompleted, OnSeekCompleted)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaError, OnMediaError)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaVideoSizeChanged,
                        OnVideoSizeChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaTimeUpdate, OnTimeUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaPlayerReleased,
                        OnMediaPlayerReleased)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_ConnectedToRemoteDevice,
                        OnConnectedToRemoteDevice)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DisconnectedFromRemoteDevice,
                        OnDisconnectedFromRemoteDevice)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidEnterFullscreen, OnDidEnterFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidExitFullscreen, OnDidExitFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidMediaPlayerPlay, OnPlayerPlay)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidMediaPlayerPause, OnPlayerPause)
    IPC_MESSAGE_HANDLER(MediaKeysMsg_KeyAdded, OnKeyAdded)
    IPC_MESSAGE_HANDLER(MediaKeysMsg_KeyError, OnKeyError)
    IPC_MESSAGE_HANDLER(MediaKeysMsg_KeyMessage, OnKeyMessage)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererMediaPlayerManager::Initialize(
    MediaPlayerHostMsg_Initialize_Type type,
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    int demuxer_client_id) {
  Send(new MediaPlayerHostMsg_Initialize(
      routing_id(), type, player_id, url, first_party_for_cookies,
      demuxer_client_id));
}

void RendererMediaPlayerManager::Start(int player_id) {
  Send(new MediaPlayerHostMsg_Start(routing_id(), player_id));
}

void RendererMediaPlayerManager::Pause(
    int player_id,
    bool is_media_related_action) {
  Send(new MediaPlayerHostMsg_Pause(
      routing_id(), player_id, is_media_related_action));
}

void RendererMediaPlayerManager::Seek(
    int player_id,
    const base::TimeDelta& time) {
  Send(new MediaPlayerHostMsg_Seek(routing_id(), player_id, time));
}

void RendererMediaPlayerManager::SetVolume(int player_id, double volume) {
  Send(new MediaPlayerHostMsg_SetVolume(routing_id(), player_id, volume));
}

void RendererMediaPlayerManager::ReleaseResources(int player_id) {
  Send(new MediaPlayerHostMsg_Release(routing_id(), player_id));
}

void RendererMediaPlayerManager::DestroyPlayer(int player_id) {
  Send(new MediaPlayerHostMsg_DestroyMediaPlayer(routing_id(), player_id));
}

void RendererMediaPlayerManager::OnMediaMetadataChanged(
    int player_id,
    base::TimeDelta duration,
    int width,
    int height,
    bool success) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaMetadataChanged(duration, width, height, success);
}

void RendererMediaPlayerManager::OnMediaPlaybackCompleted(int player_id) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnPlaybackComplete();
}

void RendererMediaPlayerManager::OnMediaBufferingUpdate(int player_id,
                                                        int percent) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnBufferingUpdate(percent);
}

void RendererMediaPlayerManager::OnSeekRequest(
    int player_id,
    const base::TimeDelta& time_to_seek) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnSeekRequest(time_to_seek);
}

void RendererMediaPlayerManager::OnSeekCompleted(
    int player_id,
    const base::TimeDelta& current_time) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnSeekComplete(current_time);
}

void RendererMediaPlayerManager::OnMediaError(int player_id, int error) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaError(error);
}

void RendererMediaPlayerManager::OnVideoSizeChanged(int player_id,
                                                    int width,
                                                    int height) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnVideoSizeChanged(width, height);
}

void RendererMediaPlayerManager::OnTimeUpdate(int player_id,
                                              base::TimeDelta current_time) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnTimeUpdate(current_time);
}

void RendererMediaPlayerManager::OnMediaPlayerReleased(int player_id) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnPlayerReleased();
}

void RendererMediaPlayerManager::OnConnectedToRemoteDevice(int player_id) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnConnectedToRemoteDevice();
}

void RendererMediaPlayerManager::OnDisconnectedFromRemoteDevice(int player_id) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnDisconnectedFromRemoteDevice();
}

void RendererMediaPlayerManager::OnDidEnterFullscreen(int player_id) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnDidEnterFullscreen();
}

void RendererMediaPlayerManager::OnDidExitFullscreen(int player_id) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnDidExitFullscreen();
}

void RendererMediaPlayerManager::OnPlayerPlay(int player_id) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaPlayerPlay();
}

void RendererMediaPlayerManager::OnPlayerPause(int player_id) {
  WebMediaPlayerAndroid* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaPlayerPause();
}

void RendererMediaPlayerManager::EnterFullscreen(int player_id) {
  Send(new MediaPlayerHostMsg_EnterFullscreen(routing_id(), player_id));
}

void RendererMediaPlayerManager::ExitFullscreen(int player_id) {
  Send(new MediaPlayerHostMsg_ExitFullscreen(routing_id(), player_id));
}

void RendererMediaPlayerManager::InitializeCDM(int media_keys_id,
                                               ProxyMediaKeys* media_keys,
                                               const std::vector<uint8>& uuid,
                                               const GURL& frame_url) {
  RegisterMediaKeys(media_keys_id, media_keys);
  Send(new MediaKeysHostMsg_InitializeCDM(
      routing_id(), media_keys_id, uuid, frame_url));
}

void RendererMediaPlayerManager::GenerateKeyRequest(
    int media_keys_id,
    const std::string& type,
    const std::vector<uint8>& init_data) {
  Send(new MediaKeysHostMsg_GenerateKeyRequest(
      routing_id(), media_keys_id, type, init_data));
}

void RendererMediaPlayerManager::AddKey(int media_keys_id,
                                        const std::vector<uint8>& key,
                                        const std::vector<uint8>& init_data,
                                        const std::string& session_id) {
  Send(new MediaKeysHostMsg_AddKey(
      routing_id(), media_keys_id, key, init_data, session_id));
}

void RendererMediaPlayerManager::CancelKeyRequest(
    int media_keys_id,
    const std::string& session_id) {
  Send(new MediaKeysHostMsg_CancelKeyRequest(
      routing_id(), media_keys_id, session_id));
}

void RendererMediaPlayerManager::OnKeyAdded(int media_keys_id,
                                            const std::string& session_id) {
  ProxyMediaKeys* media_keys = GetMediaKeys(media_keys_id);
  if (media_keys)
    media_keys->OnKeyAdded(session_id);
}

void RendererMediaPlayerManager::OnKeyError(
    int media_keys_id,
    const std::string& session_id,
    media::MediaKeys::KeyError error_code,
    int system_code) {
  ProxyMediaKeys* media_keys = GetMediaKeys(media_keys_id);
  if (media_keys)
    media_keys->OnKeyError(session_id, error_code, system_code);
}

void RendererMediaPlayerManager::OnKeyMessage(
    int media_keys_id,
    const std::string& session_id,
    const std::vector<uint8>& message,
    const std::string& destination_url) {
  ProxyMediaKeys* media_keys = GetMediaKeys(media_keys_id);
  if (media_keys)
    media_keys->OnKeyMessage(session_id, message, destination_url);
}

int RendererMediaPlayerManager::RegisterMediaPlayer(
    WebMediaPlayerAndroid* player) {
  media_players_[next_media_player_id_] = player;
  return next_media_player_id_++;
}

void RendererMediaPlayerManager::UnregisterMediaPlayer(int player_id) {
  media_players_.erase(player_id);
  media_keys_.erase(player_id);
}

void RendererMediaPlayerManager::RegisterMediaKeys(int media_keys_id,
                                                   ProxyMediaKeys* media_keys) {
  // WebMediaPlayerAndroid must have already been registered for
  // |media_keys_id|. For now |media_keys_id| is the same as player_id
  // used in other methods.
  DCHECK(media_players_.find(media_keys_id) != media_players_.end());

  // Only allowed to register once.
  DCHECK(media_keys_.find(media_keys_id) == media_keys_.end());

  media_keys_[media_keys_id] = media_keys;
}

void RendererMediaPlayerManager::ReleaseVideoResources() {
  std::map<int, WebMediaPlayerAndroid*>::iterator player_it;
  for (player_it = media_players_.begin();
      player_it != media_players_.end(); ++player_it) {
    WebMediaPlayerAndroid* player = player_it->second;

    // Do not release if an audio track is still playing
    if (player && (player->paused() || player->hasVideo()))
      player->ReleaseMediaResources();
  }
}

WebMediaPlayerAndroid* RendererMediaPlayerManager::GetMediaPlayer(
    int player_id) {
  std::map<int, WebMediaPlayerAndroid*>::iterator iter =
      media_players_.find(player_id);
  if (iter != media_players_.end())
    return iter->second;
  return NULL;
}

ProxyMediaKeys* RendererMediaPlayerManager::GetMediaKeys(int media_keys_id) {
  std::map<int, ProxyMediaKeys*>::iterator iter =
      media_keys_.find(media_keys_id);
  return (iter != media_keys_.end()) ? iter->second : NULL;
}

bool RendererMediaPlayerManager::CanEnterFullscreen(WebKit::WebFrame* frame) {
  return !fullscreen_frame_ || IsInFullscreen(frame);
}

void RendererMediaPlayerManager::DidEnterFullscreen(WebKit::WebFrame* frame) {
  fullscreen_frame_ = frame;
}

void RendererMediaPlayerManager::DidExitFullscreen() {
  fullscreen_frame_ = NULL;
}

bool RendererMediaPlayerManager::IsInFullscreen(WebKit::WebFrame* frame) {
  return fullscreen_frame_ == frame;
}

#if defined(GOOGLE_TV)
void RendererMediaPlayerManager::RequestExternalSurface(
    int player_id,
    const gfx::RectF& geometry) {
  Send(new MediaPlayerHostMsg_NotifyExternalSurface(
      routing_id(), player_id, true, geometry));
}

void RendererMediaPlayerManager::DidCommitCompositorFrame() {
  std::map<int, gfx::RectF> geometry_change;
  RetrieveGeometryChanges(&geometry_change);
  for (std::map<int, gfx::RectF>::iterator it = geometry_change.begin();
       it != geometry_change.end();
       ++it) {
    Send(new MediaPlayerHostMsg_NotifyExternalSurface(
        routing_id(), it->first, false, it->second));
  }
}

void RendererMediaPlayerManager::RetrieveGeometryChanges(
    std::map<int, gfx::RectF>* changes) {
  DCHECK(changes->empty());
  for (std::map<int, WebMediaPlayerAndroid*>::iterator player_it =
           media_players_.begin();
       player_it != media_players_.end();
       ++player_it) {
    WebMediaPlayerAndroid* player = player_it->second;

    if (player && player->hasVideo()) {
      gfx::RectF rect;
      if (player->RetrieveGeometryChange(&rect)) {
        (*changes)[player_it->first] = rect;
      }
    }
  }
}
#endif

}  // namespace content
