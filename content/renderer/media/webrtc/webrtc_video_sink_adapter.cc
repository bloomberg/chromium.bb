// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_sink_adapter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/media/native_handle_impl.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "ui/gfx/size.h"

using media::CopyYPlane;
using media::CopyUPlane;
using media::CopyVPlane;

namespace content {

WebRtcVideoSinkAdapter::WebRtcVideoSinkAdapter(
    webrtc::VideoTrackInterface* video_track,
    MediaStreamVideoSink* sink)
    : message_loop_proxy_(base::MessageLoopProxy::current()),
      sink_(sink),
      video_track_(video_track),
      state_(video_track->state()),
      track_enabled_(video_track->enabled()) {
  DCHECK(sink);
  video_track_->AddRenderer(this);
  video_track_->RegisterObserver(this);
  DVLOG(1) << "WebRtcVideoSinkAdapter";
}

WebRtcVideoSinkAdapter::~WebRtcVideoSinkAdapter() {
  video_track_->RemoveRenderer(this);
  video_track_->UnregisterObserver(this);
  DVLOG(1) << "~WebRtcVideoSinkAdapter";
}

void WebRtcVideoSinkAdapter::SetSize(int width, int height) {
}

void WebRtcVideoSinkAdapter::RenderFrame(const cricket::VideoFrame* frame) {
  base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(
      frame->GetTimeStamp() / talk_base::kNumNanosecsPerMillisec);

  scoped_refptr<media::VideoFrame> video_frame;
  if (frame->GetNativeHandle() != NULL) {
    NativeHandleImpl* handle =
        static_cast<NativeHandleImpl*>(frame->GetNativeHandle());
    video_frame = static_cast<media::VideoFrame*>(handle->GetHandle());
    video_frame->SetTimestamp(timestamp);
  } else {
    gfx::Size size(frame->GetWidth(), frame->GetHeight());
    video_frame = media::VideoFrame::CreateFrame(
        media::VideoFrame::YV12, size, gfx::Rect(size), size, timestamp);

    // Aspect ratio unsupported; DCHECK when there are non-square pixels.
    DCHECK_EQ(frame->GetPixelWidth(), 1u);
    DCHECK_EQ(frame->GetPixelHeight(), 1u);

    int y_rows = frame->GetHeight();
    int uv_rows = frame->GetChromaHeight();
    CopyYPlane(
        frame->GetYPlane(), frame->GetYPitch(), y_rows, video_frame.get());
    CopyUPlane(
        frame->GetUPlane(), frame->GetUPitch(), uv_rows, video_frame.get());
    CopyVPlane(
        frame->GetVPlane(), frame->GetVPitch(), uv_rows, video_frame.get());
  }

  message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&WebRtcVideoSinkAdapter::DoRenderFrameOnMainThread,
                            AsWeakPtr(), video_frame));
}

void WebRtcVideoSinkAdapter::OnChanged() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  // TODO(perkj): OnChanged belongs to the base class of WebRtcVideoSinkAdapter
  // common for both webrtc audio and video.
  webrtc::MediaStreamTrackInterface::TrackState state = video_track_->state();
  if (state != state_) {
    state_ = state;
    switch (state) {
      case webrtc::MediaStreamTrackInterface::kInitializing:
        // Ignore the kInitializing state since there is no match in
        // WebMediaStreamSource::ReadyState.
        break;
      case webrtc::MediaStreamTrackInterface::kLive:
        sink_->OnReadyStateChanged(blink::WebMediaStreamSource::ReadyStateLive);
        break;
      case webrtc::MediaStreamTrackInterface::kEnded:
        sink_->OnReadyStateChanged(
            blink::WebMediaStreamSource::ReadyStateEnded);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  if (track_enabled_ != video_track_->enabled()) {
    track_enabled_ = video_track_->enabled();
    sink_->OnEnabledChanged(track_enabled_);
  }
}

void WebRtcVideoSinkAdapter::DoRenderFrameOnMainThread(
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  sink_->OnVideoFrame(video_frame);
}

}  // namespace content
