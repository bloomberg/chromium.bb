// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_source.h"

#include "base/logging.h"
#include "content/public/renderer/media_stream_video_sink.h"

namespace content {

void MediaStreamVideoSource::AddTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints) {
  // TODO(ronghuawu): Put |track| in the registered tracks list. Will later
  // deliver frames to it according to |constraints|.
}

void MediaStreamVideoSource::RemoveTrack(
    const blink::WebMediaStreamTrack& track) {
  // TODO(ronghuawu): Remove |track| from the list, i.e. will stop delivering
  // frame to |track|.
}

void MediaStreamVideoSource::SetReadyState(
    blink::WebMediaStreamSource::ReadyState state) {
  // TODO(ronghuawu): Sets WebMediaStreamSource's ready state and notifies the
  // ready state to all registered tracks.
}

void MediaStreamVideoSource::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  // TODO(ronghuawu): Deliver |frame| to all the registered tracks.
}

MediaStreamVideoSource::~MediaStreamVideoSource() {
}

}  // namespace content
