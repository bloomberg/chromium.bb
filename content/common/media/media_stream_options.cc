// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_options.h"

#include "base/logging.h"

namespace media_stream {

StreamOptions::StreamOptions()
    : audio_type(content::MEDIA_NO_SERVICE),
      video_type(content::MEDIA_NO_SERVICE) {}

StreamOptions::StreamOptions(bool user_audio, bool user_video)
    : audio_type(user_audio ?
                     content::MEDIA_DEVICE_AUDIO_CAPTURE :
                     content::MEDIA_NO_SERVICE),
      video_type(user_video ?
                     content::MEDIA_DEVICE_VIDEO_CAPTURE :
                     content::MEDIA_NO_SERVICE) {}

StreamOptions::StreamOptions(MediaStreamType audio_type,
                             MediaStreamType video_type)
    : audio_type(audio_type), video_type(video_type) {
  DCHECK(IsAudioMediaType(audio_type) ||
         audio_type == content::MEDIA_NO_SERVICE);
  DCHECK(IsVideoMediaType(video_type) ||
         video_type == content::MEDIA_NO_SERVICE);
}

// static
const int StreamDeviceInfo::kNoId = -1;

StreamDeviceInfo::StreamDeviceInfo()
    : stream_type(content::MEDIA_NO_SERVICE),
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
