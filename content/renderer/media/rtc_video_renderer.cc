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
      state_(STOPPED),
      first_frame_rendered_(false),
      video_track_(video_track) {
}

RTCVideoRenderer::~RTCVideoRenderer() {
}

void RTCVideoRenderer::Start() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STOPPED);
  DCHECK(!first_frame_rendered_);

  AddToVideoTrack(this, video_track_);
  state_ = STARTED;

  if (video_track_.source().readyState() ==
          blink::WebMediaStreamSource::ReadyStateEnded ||
      !video_track_.isEnabled()) {
    MaybeRenderSignalingFrame();
  }
}

void RTCVideoRenderer::Stop() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(state_ == STARTED || state_ == PAUSED);
  RemoveFromVideoTrack(this, video_track_);
  state_ = STOPPED;
  first_frame_rendered_ = false;
}

void RTCVideoRenderer::Play() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == PAUSED) {
    state_ = STARTED;
  }
}

void RTCVideoRenderer::Pause() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == STARTED) {
    state_ = PAUSED;
  }
}

void RTCVideoRenderer::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state == blink::WebMediaStreamSource::ReadyStateEnded)
    MaybeRenderSignalingFrame();
}

void RTCVideoRenderer::OnEnabledChanged(bool enabled) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (!enabled)
    MaybeRenderSignalingFrame();
}

void RTCVideoRenderer::OnVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ != STARTED) {
    return;
  }

  TRACE_EVENT_INSTANT1("rtc_video_renderer",
                       "OnVideoFrame",
                       TRACE_EVENT_SCOPE_THREAD,
                       "timestamp",
                       frame->GetTimestamp().InMilliseconds());
  repaint_cb_.Run(frame);
  first_frame_rendered_ = true;
}

void RTCVideoRenderer::MaybeRenderSignalingFrame() {
  // Render a small black frame if no frame has been rendered.
  // This is necessary to make sure audio can play if the video tag src is
  // a MediaStream video track that has been rejected, ended or disabled.
  if (first_frame_rendered_)
    return;

  const int kMinFrameSize = 2;
  const gfx::Size size(kMinFrameSize, kMinFrameSize);
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(size);
  OnVideoFrame(video_frame);
}

}  // namespace content
