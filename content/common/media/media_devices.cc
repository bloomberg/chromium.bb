// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_devices.h"
#include "media/audio/audio_device_description.h"
#include "media/capture/video/video_capture_device_descriptor.h"

namespace content {

MediaDeviceInfo::MediaDeviceInfo() = default;

MediaDeviceInfo::MediaDeviceInfo(const MediaDeviceInfo& other) = default;

MediaDeviceInfo::MediaDeviceInfo(MediaDeviceInfo&& other) = default;

MediaDeviceInfo::MediaDeviceInfo(const std::string& device_id,
                                 const std::string& label,
                                 const std::string& group_id)
    : device_id(device_id), label(label), group_id(group_id) {}

MediaDeviceInfo::MediaDeviceInfo(
    const media::AudioDeviceDescription& device_description)
    : device_id(device_description.unique_id),
      label(device_description.device_name),
      group_id(device_description.group_id) {}

MediaDeviceInfo::MediaDeviceInfo(
    const media::VideoCaptureDeviceDescriptor& descriptor)
    : device_id(descriptor.device_id), label(descriptor.GetNameAndModel()) {}

MediaDeviceInfo::~MediaDeviceInfo() = default;

MediaDeviceInfo& MediaDeviceInfo::operator=(const MediaDeviceInfo& other) =
    default;

MediaDeviceInfo& MediaDeviceInfo::operator=(MediaDeviceInfo&& other) = default;

bool operator==(const MediaDeviceInfo& first, const MediaDeviceInfo& second) {
  return first.device_id == second.device_id && first.label == second.label;
}

}  // namespace content
