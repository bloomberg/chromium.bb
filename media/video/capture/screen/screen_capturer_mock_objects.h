// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_MOCK_OBJECTS_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_MOCK_OBJECTS_H_

#include "media/video/capture/screen/mouse_cursor_shape.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockScreenCapturer : public ScreenCapturer {
 public:
  MockScreenCapturer();
  virtual ~MockScreenCapturer();

  MOCK_METHOD1(Start, void(Delegate* delegate));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(InvalidateRegion, void(const SkRegion& invalid_region));
  MOCK_METHOD0(CaptureFrame, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScreenCapturer);
};

class MockScreenCapturerDelegate : public ScreenCapturer::Delegate {
 public:
  MockScreenCapturerDelegate();
  virtual ~MockScreenCapturerDelegate();

  void OnCursorShapeChanged(scoped_ptr<MouseCursorShape> cursor_shape) OVERRIDE;

  MOCK_METHOD1(CreateSharedBuffer, scoped_refptr<SharedBuffer>(uint32));
  MOCK_METHOD1(ReleaseSharedBuffer, void(scoped_refptr<SharedBuffer>));
  MOCK_METHOD1(OnCaptureCompleted, void(scoped_refptr<ScreenCaptureData>));
  MOCK_METHOD1(OnCursorShapeChangedPtr,
               void(MouseCursorShape* cursor_shape));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScreenCapturerDelegate);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_MOCK_OBJECTS_H_
