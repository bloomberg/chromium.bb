// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_renderer.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/base/timeutils.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"

using media::CopyYPlane;
using media::CopyUPlane;
using media::CopyVPlane;

namespace content {

RTCVideoRenderer::RTCVideoRenderer(
    webrtc::VideoTrackInterface* video_track,
    const base::Closure& error_cb,
    const RepaintCB& repaint_cb)
    : error_cb_(error_cb),
      repaint_cb_(repaint_cb),
      message_loop_proxy_(base::MessageLoopProxy::current()),
      state_(kStopped),
      video_track_(video_track) {
}

RTCVideoRenderer::~RTCVideoRenderer() {
}

void RTCVideoRenderer::Start() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopped);

  if (video_track_) {
    video_track_->AddRenderer(this);
    video_track_->RegisterObserver(this);
  }
  state_ = kStarted;
  MaybeRenderSignalingFrame();
}

void RTCVideoRenderer::Stop() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (video_track_) {
    state_ = kStopped;
    video_track_->RemoveRenderer(this);
    video_track_->UnregisterObserver(this);
    video_track_ = NULL;
  }
}

void RTCVideoRenderer::Play() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (video_track_ && state_ == kPaused) {
    state_ = kStarted;
  }
}

void RTCVideoRenderer::Pause() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (video_track_ && state_ == kStarted) {
    state_ = kPaused;
  }
}

void RTCVideoRenderer::SetSize(int width, int height) {
}

void RTCVideoRenderer::RenderFrame(const cricket::VideoFrame* frame) {
  base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(
      frame->GetTimeStamp() / talk_base::kNumNanosecsPerMillisec);
  gfx::Size size(frame->GetWidth(), frame->GetHeight());
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                     size,
                                     gfx::Rect(size),
                                     size,
                                     timestamp);

  // Aspect ratio unsupported; DCHECK when there are non-square pixels.
  DCHECK_EQ(frame->GetPixelWidth(), 1u);
  DCHECK_EQ(frame->GetPixelHeight(), 1u);

  int y_rows = frame->GetHeight();
  int uv_rows = frame->GetHeight() / 2;  // YV12 format.
  CopyYPlane(frame->GetYPlane(), frame->GetYPitch(), y_rows, video_frame);
  CopyUPlane(frame->GetUPlane(), frame->GetUPitch(), uv_rows, video_frame);
  CopyVPlane(frame->GetVPlane(), frame->GetVPitch(), uv_rows, video_frame);

  message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&RTCVideoRenderer::DoRenderFrameOnMainThread,
                            this, video_frame));
}

void RTCVideoRenderer::OnChanged() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  MaybeRenderSignalingFrame();
}

void RTCVideoRenderer::MaybeRenderSignalingFrame() {
  // Render a small black frame if the track transition to ended.
  // This is necessary to make sure audio can play if the video tag src is
  // a MediaStream video track that has been rejected or ended.
  if (video_track_->state() == webrtc::MediaStreamTrackInterface::kEnded) {
    const int kMinFrameSize = 2;
    const gfx::Size size(kMinFrameSize, kMinFrameSize);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateBlackFrame(size);
    DoRenderFrameOnMainThread(video_frame);
  }
}

void RTCVideoRenderer::DoRenderFrameOnMainThread(
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  if (state_ != kStarted) {
    return;
  }

  TRACE_EVENT0("video", "DoRenderFrameOnMainThread");
  repaint_cb_.Run(video_frame);
}

}  // namespace content
