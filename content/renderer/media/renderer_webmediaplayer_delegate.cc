// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webmediaplayer_delegate.h"

#include <stdint.h>

#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace media {

RendererWebMediaPlayerDelegate::RendererWebMediaPlayerDelegate(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

RendererWebMediaPlayerDelegate::~RendererWebMediaPlayerDelegate() {}

int RendererWebMediaPlayerDelegate::AddObserver(Observer* observer) {
  return id_map_.Add(observer);
}

void RendererWebMediaPlayerDelegate::RemoveObserver(int delegate_id) {
  DCHECK(id_map_.Lookup(delegate_id));
  id_map_.Remove(delegate_id);
}

void RendererWebMediaPlayerDelegate::DidPlay(int delegate_id,
                                             bool has_video,
                                             bool has_audio,
                                             bool is_remote,
                                             base::TimeDelta duration) {
  DCHECK(id_map_.Lookup(delegate_id));
  has_played_media_ = true;
  Send(new MediaPlayerDelegateHostMsg_OnMediaPlaying(
      routing_id(), delegate_id, has_video, has_audio, is_remote, duration));
}

void RendererWebMediaPlayerDelegate::DidPause(int delegate_id,
                                              bool reached_end_of_stream) {
  DCHECK(id_map_.Lookup(delegate_id));
  Send(new MediaPlayerDelegateHostMsg_OnMediaPaused(routing_id(), delegate_id,
                                                    reached_end_of_stream));
}

void RendererWebMediaPlayerDelegate::PlayerGone(int delegate_id) {
  DCHECK(id_map_.Lookup(delegate_id));
  Send(new MediaPlayerDelegateHostMsg_OnMediaDestroyed(routing_id(),
                                                       delegate_id));
}

bool RendererWebMediaPlayerDelegate::IsHidden() {
  return render_frame()->IsHidden();
}

void RendererWebMediaPlayerDelegate::WasHidden() {
  for (IDMap<Observer>::iterator it(&id_map_); !it.IsAtEnd(); it.Advance())
    it.GetCurrentValue()->OnHidden(false);
}

void RendererWebMediaPlayerDelegate::WasShown() {
  for (IDMap<Observer>::iterator it(&id_map_); !it.IsAtEnd(); it.Advance())
    it.GetCurrentValue()->OnShown();
}

bool RendererWebMediaPlayerDelegate::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererWebMediaPlayerDelegate, msg)
  IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_Pause, OnMediaDelegatePause)
  IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_Play, OnMediaDelegatePlay)
  IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_SuspendAllMediaPlayers,
                      OnMediaDelegateSuspendAllMediaPlayers)
  IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_UpdateVolumeMultiplier,
                      OnMediaDelegateVolumeMultiplierUpdate)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererWebMediaPlayerDelegate::OnMediaDelegatePause(int delegate_id) {
  Observer* observer = id_map_.Lookup(delegate_id);
  if (observer)
    observer->OnPause();
}

void RendererWebMediaPlayerDelegate::OnMediaDelegatePlay(int delegate_id) {
  Observer* observer = id_map_.Lookup(delegate_id);
  if (observer)
    observer->OnPlay();
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateSuspendAllMediaPlayers() {
  for (IDMap<Observer>::iterator it(&id_map_); !it.IsAtEnd(); it.Advance())
    it.GetCurrentValue()->OnHidden(true);
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateVolumeMultiplierUpdate(
    int delegate_id,
    double multiplier) {
  Observer* observer = id_map_.Lookup(delegate_id);
  if (observer)
    observer->OnVolumeMultiplierUpdate(multiplier);
}

}  // namespace media
