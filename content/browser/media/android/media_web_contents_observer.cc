// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_web_contents_observer.h"

#include "base/stl_util.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/common/media/cdm_messages.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

namespace content {

MediaWebContentsObserver::MediaWebContentsObserver(
    RenderViewHost* render_view_host)
    : WebContentsObserver(WebContents::FromRenderViewHost(render_view_host)) {
}

MediaWebContentsObserver::~MediaWebContentsObserver() {
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  uintptr_t key = reinterpret_cast<uintptr_t>(render_frame_host);
  media_player_managers_.erase(key);
}

bool MediaWebContentsObserver::OnMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  BrowserMediaPlayerManager* player_manager =
      GetMediaPlayerManager(render_frame_host);
  DCHECK(player_manager);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaWebContentsObserver, msg)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_EnterFullscreen,
                        player_manager,
                        BrowserMediaPlayerManager::OnEnterFullscreen)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_ExitFullscreen,
                        player_manager,
                        BrowserMediaPlayerManager::OnExitFullscreen)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Initialize,
                        player_manager,
                        BrowserMediaPlayerManager::OnInitialize)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Start,
                        player_manager,
                        BrowserMediaPlayerManager::OnStart)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Seek,
                        player_manager,
                        BrowserMediaPlayerManager::OnSeek)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Pause,
                        player_manager,
                        BrowserMediaPlayerManager::OnPause)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_SetVolume,
                        player_manager,
                        BrowserMediaPlayerManager::OnSetVolume)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_SetPoster,
                        player_manager,
                        BrowserMediaPlayerManager::OnSetPoster)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Release,
                        player_manager,
                        BrowserMediaPlayerManager::OnReleaseResources)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_DestroyMediaPlayer,
                        player_manager,
                        BrowserMediaPlayerManager::OnDestroyPlayer)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_DestroyAllMediaPlayers,
                        player_manager,
                        BrowserMediaPlayerManager::DestroyAllMediaPlayers)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_SetCdm,
                        player_manager,
                        BrowserMediaPlayerManager::OnSetCdm)
    IPC_MESSAGE_FORWARD(CdmHostMsg_InitializeCdm,
                        player_manager,
                        BrowserMediaPlayerManager::OnInitializeCdm)
    IPC_MESSAGE_FORWARD(CdmHostMsg_CreateSession,
                        player_manager,
                        BrowserMediaPlayerManager::OnCreateSession)
    IPC_MESSAGE_FORWARD(CdmHostMsg_UpdateSession,
                        player_manager,
                        BrowserMediaPlayerManager::OnUpdateSession)
    IPC_MESSAGE_FORWARD(CdmHostMsg_ReleaseSession,
                        player_manager,
                        BrowserMediaPlayerManager::OnReleaseSession)
    IPC_MESSAGE_FORWARD(CdmHostMsg_DestroyCdm,
                        player_manager,
                        BrowserMediaPlayerManager::OnDestroyCdm)
#if defined(VIDEO_HOLE)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_NotifyExternalSurface,
                        player_manager,
                        BrowserMediaPlayerManager::OnNotifyExternalSurface)
#endif  // defined(VIDEO_HOLE)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

BrowserMediaPlayerManager* MediaWebContentsObserver::GetMediaPlayerManager(
    RenderFrameHost* render_frame_host) {
  uintptr_t key = reinterpret_cast<uintptr_t>(render_frame_host);
  if (!media_player_managers_.contains(key)) {
    media_player_managers_.set(
        key,
        scoped_ptr<BrowserMediaPlayerManager>(
            BrowserMediaPlayerManager::Create(render_frame_host)));
  }
  return media_player_managers_.get(key);
}

void MediaWebContentsObserver::PauseVideo() {
  for (MediaPlayerManagerMap::iterator iter = media_player_managers_.begin();
      iter != media_player_managers_.end(); ++iter) {
    BrowserMediaPlayerManager* manager = iter->second;
    manager->PauseVideo();
  }
}

#if defined(VIDEO_HOLE)
void MediaWebContentsObserver::OnFrameInfoUpdated() {
  for (MediaPlayerManagerMap::iterator iter = media_player_managers_.begin();
      iter != media_player_managers_.end(); ++iter) {
    BrowserMediaPlayerManager* manager = iter->second;
    manager->OnFrameInfoUpdated();
  }
}
#endif  // defined(VIDEO_HOLE)

}  // namespace content
