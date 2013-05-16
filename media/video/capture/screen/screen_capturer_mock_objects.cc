// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer_mock_objects.h"

#include "media/video/capture/screen/screen_capturer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace media {

MockScreenCapturer::MockScreenCapturer() {}
MockScreenCapturer::~MockScreenCapturer() {}

MockScreenCapturerCallback::MockScreenCapturerCallback() {}
MockScreenCapturerCallback::~MockScreenCapturerCallback() {}

MockMouseShapeObserver::MockMouseShapeObserver() {}
MockMouseShapeObserver::~MockMouseShapeObserver() {}

void MockMouseShapeObserver::OnCursorShapeChanged(
    scoped_ptr<MouseCursorShape> cursor_shape) {
  // Notify the mock method.
  OnCursorShapeChangedPtr(cursor_shape.get());
}

}  // namespace media
