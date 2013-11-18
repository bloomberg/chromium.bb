// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device.h"
#include "base/strings/string_util.h"

namespace media {

const std::string VideoCaptureDevice::Name::GetNameAndModel() const {
  const std::string model_id = GetModel();
  if (model_id.empty())
    return device_name_;
  const std::string suffix = " (" + model_id + ")";
  if (EndsWith(device_name_, suffix, true))  // |true| means case-sensitive.
    return device_name_;
  return device_name_ + suffix;
}

VideoCaptureDevice::~VideoCaptureDevice() {}

}  // namespace media
