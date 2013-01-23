// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_options.h"

#include "base/logging.h"

namespace content {

const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] = "chromeMediaSourceId";
const char kMediaStreamSourceTab[] = "tab";

StreamOptions::StreamOptions()
    : audio_type(MEDIA_NO_SERVICE),
      video_type(MEDIA_NO_SERVICE) {}

StreamOptions::StreamOptions(MediaStreamType audio_type,
                             MediaStreamType video_type)
    : audio_type(audio_type), video_type(video_type) {
  DCHECK(IsAudioMediaType(audio_type) || audio_type == MEDIA_NO_SERVICE);
  DCHECK(IsVideoMediaType(video_type) || video_type == MEDIA_NO_SERVICE);
}

// static
const int StreamDeviceInfo::kNoId = -1;

StreamDeviceInfo::StreamDeviceInfo()
    : in_use(false),
      session_id(kNoId) {}

StreamDeviceInfo::StreamDeviceInfo(MediaStreamType service_param,
                                   const std::string& name_param,
                                   const std::string& device_param,
                                   bool opened)
    : device(service_param, device_param, name_param),
      in_use(opened),
      session_id(kNoId) {
}

StreamDeviceInfo::StreamDeviceInfo(MediaStreamType service_param,
                                   const std::string& name_param,
                                   const std::string& device_param,
                                   int sample_rate,
                                   int channel_layout,
                                   bool opened)
    : device(service_param, device_param, name_param, sample_rate,
             channel_layout),
      in_use(opened),
      session_id(kNoId) {
}

// static
bool StreamDeviceInfo::IsEqual(const StreamDeviceInfo& first,
                               const StreamDeviceInfo& second) {
  return first.device.type == second.device.type &&
      first.device.name == second.device.name &&
      first.device.id == second.device.id &&
      first.device.sample_rate == second.device.sample_rate &&
      first.device.channel_layout == second.device.channel_layout &&
      first.in_use == second.in_use &&
      first.session_id == second.session_id;
}

}  // namespace content
