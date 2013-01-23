// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/media_stream_request.h"

#include "base/logging.h"

namespace content {

bool IsAudioMediaType(MediaStreamType type) {
  return (type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
          type == content::MEDIA_TAB_AUDIO_CAPTURE);
}

bool IsVideoMediaType(MediaStreamType type) {
  return (type == content::MEDIA_DEVICE_VIDEO_CAPTURE ||
          type == content::MEDIA_TAB_VIDEO_CAPTURE);
}

MediaStreamDevice::MediaStreamDevice() : type(MEDIA_NO_SERVICE) {}

MediaStreamDevice::MediaStreamDevice(
    MediaStreamType type,
    const std::string& id,
    const std::string& name)
    : type(type),
      id(id),
      name(name),
      sample_rate(0),
      channel_layout(0) {
}

MediaStreamDevice::MediaStreamDevice(
    MediaStreamType type,
    const std::string& id,
    const std::string& name,
    int sample_rate,
    int channel_layout)
    : type(type),
      id(id),
      name(name),
      sample_rate(sample_rate),
      channel_layout(channel_layout) {
}

MediaStreamDevice::~MediaStreamDevice() {}

MediaStreamRequest::MediaStreamRequest(
    int render_process_id,
    int render_view_id,
    const GURL& security_origin,
    MediaStreamRequestType request_type,
    const std::string& requested_device_id,
    MediaStreamType audio_type,
    MediaStreamType video_type)
    : render_process_id(render_process_id),
      render_view_id(render_view_id),
      security_origin(security_origin),
      request_type(request_type),
      requested_device_id(requested_device_id),
      audio_type(audio_type),
      video_type(video_type) {
}

MediaStreamRequest::~MediaStreamRequest() {}

}  // namespace content
