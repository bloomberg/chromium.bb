// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webmediaplayer_delegate.h"

#include <stdint.h>

#include "content/common/frame_messages.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace media {

RendererWebMediaPlayerDelegate::RendererWebMediaPlayerDelegate(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

RendererWebMediaPlayerDelegate::~RendererWebMediaPlayerDelegate() {}

void RendererWebMediaPlayerDelegate::DidPlay(blink::WebMediaPlayer* player) {
  has_played_media_ = true;
  Send(new FrameHostMsg_MediaPlayingNotification(
      routing_id(), reinterpret_cast<int64_t>(player), player->hasVideo(),
      player->hasAudio(), player->isRemote()));
}

void RendererWebMediaPlayerDelegate::DidPause(blink::WebMediaPlayer* player) {
  Send(new FrameHostMsg_MediaPausedNotification(
      routing_id(), reinterpret_cast<int64_t>(player)));
}

void RendererWebMediaPlayerDelegate::PlayerGone(blink::WebMediaPlayer* player) {
  DidPause(player);
}

void RendererWebMediaPlayerDelegate::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void RendererWebMediaPlayerDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void RendererWebMediaPlayerDelegate::WasHidden() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnHidden());
}

void RendererWebMediaPlayerDelegate::WasShown() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnShown());
}

bool RendererWebMediaPlayerDelegate::IsHidden() {
  return render_frame()->IsHidden();
}

}  // namespace media
