// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_renderer.h"

#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"

namespace content {

RTCVideoRenderer::RTCVideoRenderer(
    const blink::WebMediaStreamTrack& video_track,
    const base::Closure& error_cb,
    const RepaintCB& repaint_cb)
    : error_cb_(error_cb),
      repaint_cb_(repaint_cb),
      message_loop_proxy_(base::MessageLoopProxy::current()),
      state_(kStopped),
      video_track_(video_track) {
  MaybeRenderSignalingFrame(video_track_.source().readyState());
}

RTCVideoRenderer::~RTCVideoRenderer() {
}

void RTCVideoRenderer::Start() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopped);

  AddToVideoTrack(this, video_track_);
  state_ = kStarted;
}

void RTCVideoRenderer::Stop() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  RemoveFromVideoTrack(this, video_track_);
}

void RTCVideoRenderer::Play() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == kPaused) {
    state_ = kStarted;
  }
}

void RTCVideoRenderer::Pause() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == kStarted) {
    state_ = kPaused;
  }
}

void RTCVideoRenderer::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  MaybeRenderSignalingFrame(state);
}

void RTCVideoRenderer::OnVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ != kStarted) {
    return;
  }

  TRACE_EVENT_INSTANT1("rtc_video_renderer",
                       "OnVideoFrame",
                       TRACE_EVENT_SCOPE_THREAD,
                       "timestamp",
                       frame->GetTimestamp().InMilliseconds());
  repaint_cb_.Run(frame);
}

void RTCVideoRenderer::MaybeRenderSignalingFrame(
    blink::WebMediaStreamSource::ReadyState state) {
  // Render a small black frame if the track transition to ended.
  // This is necessary to make sure audio can play if the video tag src is
  // a MediaStream video track that has been rejected or ended.
  if (state == blink::WebMediaStreamSource::ReadyStateEnded) {
    const int kMinFrameSize = 2;
    const gfx::Size size(kMinFrameSize, kMinFrameSize);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateBlackFrame(size);
    OnVideoFrame(video_frame);
  }
}

}  // namespace content
