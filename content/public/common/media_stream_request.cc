// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/media_stream_request.h"

namespace content {

MediaStreamDevice::MediaStreamDevice(
    MediaStreamDeviceType type,
    const std::string& device_id,
    const std::string& name)
    : type(type),
      device_id(device_id),
      name(name) {
}

MediaStreamRequest::MediaStreamRequest(
    int render_process_id,
    int render_view_id,
    const GURL& security_origin)
    : render_process_id(render_process_id),
      render_view_id(render_view_id),
      security_origin(security_origin) {
}

MediaStreamRequest::~MediaStreamRequest() {
}

}  // namespace content
