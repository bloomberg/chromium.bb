// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/media_stream_request.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace content {

bool IsAudioInputMediaType(MediaStreamType type) {
  return (type == MEDIA_DEVICE_AUDIO_CAPTURE ||
          type == MEDIA_TAB_AUDIO_CAPTURE ||
          type == MEDIA_DESKTOP_AUDIO_CAPTURE);
}

bool IsVideoMediaType(MediaStreamType type) {
  return (type == MEDIA_DEVICE_VIDEO_CAPTURE ||
          type == MEDIA_TAB_VIDEO_CAPTURE ||
          type == MEDIA_DESKTOP_VIDEO_CAPTURE);
}

bool IsScreenCaptureMediaType(MediaStreamType type) {
  return (type == MEDIA_TAB_AUDIO_CAPTURE ||
          type == MEDIA_TAB_VIDEO_CAPTURE ||
          type == MEDIA_DESKTOP_AUDIO_CAPTURE ||
          type == MEDIA_DESKTOP_VIDEO_CAPTURE);
}

// static
const int MediaStreamDevice::kNoId = -1;

MediaStreamDevice::MediaStreamDevice()
    : type(MEDIA_NO_SERVICE), video_facing(media::MEDIA_VIDEO_FACING_NONE) {}

MediaStreamDevice::MediaStreamDevice(MediaStreamType type,
                                     const std::string& id,
                                     const std::string& name)
    : type(type),
      id(id),
      video_facing(media::MEDIA_VIDEO_FACING_NONE),
      name(name) {
#if defined(OS_ANDROID)
  if (name.find("front") != std::string::npos) {
    video_facing = media::MEDIA_VIDEO_FACING_USER;
  } else if (name.find("back") != std::string::npos) {
    video_facing = media::MEDIA_VIDEO_FACING_ENVIRONMENT;
  }
#endif
}

MediaStreamDevice::MediaStreamDevice(MediaStreamType type,
                                     const std::string& id,
                                     const std::string& name,
                                     media::VideoFacingMode facing)
    : type(type), id(id), video_facing(facing), name(name) {}

MediaStreamDevice::MediaStreamDevice(MediaStreamType type,
                                     const std::string& id,
                                     const std::string& name,
                                     int sample_rate,
                                     int channel_layout,
                                     int frames_per_buffer)
    : type(type),
      id(id),
      video_facing(media::MEDIA_VIDEO_FACING_NONE),
      name(name),
      input(media::AudioParameters::AUDIO_FAKE,
            static_cast<media::ChannelLayout>(channel_layout),
            sample_rate,
            16,
            frames_per_buffer) {
  DCHECK(input.IsValid());
}

MediaStreamDevice::MediaStreamDevice(const MediaStreamDevice& other) = default;

MediaStreamDevice::~MediaStreamDevice() {}

bool MediaStreamDevice::IsSameDevice(
    const MediaStreamDevice& other_device) const {
  return type == other_device.type && name == other_device.name &&
         id == other_device.id &&
         input.sample_rate() == other_device.input.sample_rate() &&
         input.channel_layout() == other_device.input.channel_layout() &&
         session_id == other_device.session_id;
}

MediaStreamRequest::MediaStreamRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    bool user_gesture,
    MediaStreamRequestType request_type,
    const std::string& requested_audio_device_id,
    const std::string& requested_video_device_id,
    MediaStreamType audio_type,
    MediaStreamType video_type,
    bool disable_local_echo)
    : render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      page_request_id(page_request_id),
      security_origin(security_origin),
      user_gesture(user_gesture),
      request_type(request_type),
      requested_audio_device_id(requested_audio_device_id),
      requested_video_device_id(requested_video_device_id),
      audio_type(audio_type),
      video_type(video_type),
      disable_local_echo(disable_local_echo),
      all_ancestors_have_same_origin(false) {}

MediaStreamRequest::MediaStreamRequest(const MediaStreamRequest& other) =
    default;

MediaStreamRequest::~MediaStreamRequest() {}

}  // namespace content
