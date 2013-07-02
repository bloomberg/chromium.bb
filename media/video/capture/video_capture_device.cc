// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device.h"

namespace media {

VideoCaptureDevice::Name*
VideoCaptureDevice::Names::FindById(const std::string& id) {
  for (iterator it = begin(); it != end(); ++it) {
    if (it->id() == id)
      return &(*it);
  }
  return NULL;
}

}  // namespace media
