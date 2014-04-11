// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_video_sink.h"

namespace content {

MockMediaStreamVideoSink::MockMediaStreamVideoSink()
    : number_of_frames_(0),
      enabled_(true),
      format_(media::VideoFrame::UNKNOWN),
      state_(blink::WebMediaStreamSource::ReadyStateLive) {
}

void MockMediaStreamVideoSink::OnVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  ++number_of_frames_;
  format_ = frame->format();
  frame_size_ = frame->visible_rect().size();
}

void MockMediaStreamVideoSink::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  state_ = state;
}

void MockMediaStreamVideoSink::OnEnabledChanged(bool enabled) {
  enabled_ = enabled;
}

}  // namespace content
