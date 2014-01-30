// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_capturer_source.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_video_capturer.h"

namespace content {

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const StreamDeviceInfo& device_info,
    const SourceStoppedCallback& stop_callback,
    MediaStreamDependencyFactory* factory)
    : MediaStreamVideoSource(factory) {
  SetDeviceInfo(device_info);
  SetStopCallback(stop_callback);
}

MediaStreamVideoCapturerSource::~MediaStreamVideoCapturerSource() {
}

void MediaStreamVideoCapturerSource::InitAdapter(
    const blink::WebMediaConstraints& constraints) {
  // Create the webrtc::VideoSource implementation.
  RTCMediaConstraints webrtc_constraints(constraints);
  cricket::VideoCapturer* capturer =
      factory()->CreateVideoCapturer(device_info());
  SetAdapter(factory()->CreateVideoSource(capturer,
                                          &webrtc_constraints));
}

}  // namespace content
