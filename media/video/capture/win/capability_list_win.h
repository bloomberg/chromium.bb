// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// DirectShow is used for capturing. DirectShow provide its own threads
// for capturing.

#ifndef MEDIA_VIDEO_CAPTURE_WIN_CAPABILITY_LIST_WIN_H_
#define MEDIA_VIDEO_CAPTURE_WIN_CAPABILITY_LIST_WIN_H_

#include <list>

#include "media/video/capture/video_capture_types.h"

namespace media {

struct CapabilityWin {
  CapabilityWin(int index, const VideoCaptureFormat& format)
      : stream_index(index), supported_format(format) {}
  int stream_index;
  VideoCaptureFormat supported_format;
};

typedef std::list<CapabilityWin> CapabilityList;

CapabilityWin GetBestMatchedCapability(const VideoCaptureFormat& requested,
                                       const CapabilityList& capabilities);

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_WIN_CAPABILITY_LIST_WIN_H_
