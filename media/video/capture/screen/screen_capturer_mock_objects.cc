// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer_mock_objects.h"

#include "media/video/capture/screen/screen_capturer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

MockScreenCapturer::MockScreenCapturer() {
}

MockScreenCapturer::~MockScreenCapturer() {
}

MockScreenCapturerDelegate::MockScreenCapturerDelegate() {
}

MockScreenCapturerDelegate::~MockScreenCapturerDelegate() {
}

void MockScreenCapturerDelegate::OnCursorShapeChanged(
    scoped_ptr<MouseCursorShape> cursor_shape) {
  // Notify the mock method.
  OnCursorShapeChangedPtr(cursor_shape.get());
}

}  // namespace media
