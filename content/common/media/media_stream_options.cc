// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_options.h"

namespace media_stream {

// static
const int StreamDeviceInfo::kNoId = -1;

StreamDeviceInfo::StreamDeviceInfo()
    : stream_type(content::MEDIA_STREAM_DEVICE_TYPE_NO_SERVICE),
      in_use(false),
      session_id(kNoId) {}

StreamDeviceInfo::StreamDeviceInfo(MediaStreamType service_param,
                                   const std::string& name_param,
                                   const std::string& device_param,
                                   bool opened)
    : stream_type(service_param),
      name(name_param),
      device_id(device_param),
      in_use(opened),
      session_id(kNoId) {}

// static
bool StreamDeviceInfo::IsEqual(const StreamDeviceInfo& first,
                               const StreamDeviceInfo& second) {
  return first.stream_type == second.stream_type &&
      first.name == second.name &&
      first.device_id == second.device_id &&
      first.in_use == second.in_use &&
      first.session_id == second.session_id;
}

}  // namespace media_stream
