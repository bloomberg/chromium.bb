// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webmediaplayer_delegate.h"

#include "content/common/frame_messages.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace media {

RendererWebMediaPlayerDelegate::RendererWebMediaPlayerDelegate(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

RendererWebMediaPlayerDelegate::~RendererWebMediaPlayerDelegate() {}

void RendererWebMediaPlayerDelegate::DidPlay(blink::WebMediaPlayer* player) {
  has_played_media_ = true;
  Send(new FrameHostMsg_MediaPlayingNotification(
      routing_id(), reinterpret_cast<int64>(player), player->hasVideo(),
      player->hasAudio(), player->isRemote()));
}

void RendererWebMediaPlayerDelegate::DidPause(blink::WebMediaPlayer* player) {
  Send(new FrameHostMsg_MediaPausedNotification(
      routing_id(), reinterpret_cast<int64>(player)));
}

void RendererWebMediaPlayerDelegate::PlayerGone(blink::WebMediaPlayer* player) {
  DidPause(player);
}

}  // namespace media
