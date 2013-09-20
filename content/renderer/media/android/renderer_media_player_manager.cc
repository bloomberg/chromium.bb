// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/renderer_media_player_manager.h"

#include "content/renderer/media/android/webmediaplayer_android.h"
#include "ui/gfx/rect_f.h"

namespace content {

RendererMediaPlayerManager::RendererMediaPlayerManager()
    : next_media_player_id_(0),
      fullscreen_frame_(NULL) {
}

RendererMediaPlayerManager::~RendererMediaPlayerManager() {
  std::map<int, WebMediaPlayerAndroid*>::iterator player_it;
  for (player_it = media_players_.begin();
      player_it != media_players_.end(); ++player_it) {
    WebMediaPlayerAndroid* player = player_it->second;
    player->Detach();
  }
}

int RendererMediaPlayerManager::RegisterMediaPlayer(
    WebMediaPlayerAndroid* player) {
  media_players_[next_media_player_id_] = player;
  return next_media_player_id_++;
}

void RendererMediaPlayerManager::UnregisterMediaPlayer(int player_id) {
  media_players_.erase(player_id);
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
