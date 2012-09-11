// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/media_stream_request.h"

namespace content {

bool IsAudioMediaType(MediaStreamDeviceType type) {
  return (type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
          type == content::MEDIA_TAB_AUDIO_CAPTURE);
}

bool IsVideoMediaType(MediaStreamDeviceType type) {
  return (type == content::MEDIA_DEVICE_VIDEO_CAPTURE ||
          type == content::MEDIA_TAB_VIDEO_CAPTURE);
}

MediaStreamDevice::MediaStreamDevice(
    MediaStreamDeviceType type,
    const std::string& device_id,
    const std::string& name)
    : type(type),
      device_id(device_id),
      name(name) {
}

MediaStreamDevice::~MediaStreamDevice() {}

MediaStreamRequest::MediaStreamRequest(
    int render_process_id,
    int render_view_id,
    const GURL& security_origin)
    : render_process_id(render_process_id),
      render_view_id(render_view_id),
      security_origin(security_origin) {
}

MediaStreamRequest::~MediaStreamRequest() {
  for (MediaStreamDeviceMap::iterator iter = devices.begin();
       iter != devices.end();
       ++iter) {
    iter->second.clear();
  }
  devices.clear();
}

}  // namespace content
