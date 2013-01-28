// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capture_frame.h"

namespace media {

ScreenCaptureFrame::~ScreenCaptureFrame() {
}

ScreenCaptureFrame::ScreenCaptureFrame()
    : bytes_per_row_(0),
      dimensions_(SkISize::Make(0, 0)),
      pixels_(NULL) {
}

}  // namespace media
