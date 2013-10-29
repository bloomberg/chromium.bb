// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_PLAYER_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_PLAYER_MANAGER_H_

#include <map>

#include "base/basictypes.h"

namespace WebKit {
class WebFrame;
}

namespace gfx {
class RectF;
}

namespace content {

class ProxyMediaKeys;
class WebMediaPlayerAndroid;

// Class for managing all the WebMediaPlayerAndroid objects in the same
// RenderView.
class RendererMediaPlayerManager {
 public:
  RendererMediaPlayerManager();
  virtual ~RendererMediaPlayerManager();

  // Register and unregister a WebMediaPlayerAndroid object.
  int RegisterMediaPlayer(WebMediaPlayerAndroid* player);
  void UnregisterMediaPlayer(int player_id);

  // Register a ProxyMediaKeys object. There must be a WebMediaPlayerAndroid
  // object already registered for this id, and it is unregistered when the
  // player is unregistered. For now |media_keys_id| is the same as player_id
  // used in other methods.
  void RegisterMediaKeys(int media_keys_id, ProxyMediaKeys* media_keys);

  // Release the media resources managed by this object when a video
  // is playing.
  void ReleaseVideoResources();

  // Check whether a player can enter fullscreen.
  bool CanEnterFullscreen(WebKit::WebFrame* frame);

  // Called when a player entered or exited fullscreen.
  void DidEnterFullscreen(WebKit::WebFrame* frame);
  void DidExitFullscreen();

  // Check whether the Webframe is in fullscreen.
  bool IsInFullscreen(WebKit::WebFrame* frame);

  // Get the pointer to WebMediaPlayerAndroid given the |player_id|.
  WebMediaPlayerAndroid* GetMediaPlayer(int player_id);

  // Get the pointer to ProxyMediaKeys given the |media_keys_id|.
  ProxyMediaKeys* GetMediaKeys(int media_keys_id);

#if defined(GOOGLE_TV)
  // Get the list of media players with video geometry changes.
  void RetrieveGeometryChanges(std::map<int, gfx::RectF>* changes);
#endif

 private:
  // Info for all available WebMediaPlayerAndroid on a page; kept so that
  // we can enumerate them to send updates about tab focus and visibily.
  std::map<int, WebMediaPlayerAndroid*> media_players_;

  // Info for all available ProxyMediaKeys. There must be at most one
  // ProxyMediaKeys for each available WebMediaPlayerAndroid.
  std::map<int, ProxyMediaKeys*> media_keys_;

  int next_media_player_id_;

  // WebFrame of the fullscreen video.
  WebKit::WebFrame* fullscreen_frame_;

  DISALLOW_COPY_AND_ASSIGN(RendererMediaPlayerManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_PLAYER_MANAGER_H_
