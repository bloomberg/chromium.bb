// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device.h"

namespace media {

const std::string VideoCaptureDevice::Name::GetNameAndModel() const {
// On Linux, the device name already includes the model identifier.
#if !defined(OS_LINUX)
  std::string model_id = GetModel();
  if (!model_id.empty())
    return device_name_ + " (" + model_id + ")";
#endif  // if !defined(OS_LINUX)
  return device_name_;
}

VideoCaptureDevice::Name*
VideoCaptureDevice::Names::FindById(const std::string& id) {
  for (iterator it = begin(); it != end(); ++it) {
    if (it->id() == id)
      return &(*it);
  }
  return NULL;
}

}  // namespace media
