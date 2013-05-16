// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_MOCK_OBJECTS_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_MOCK_OBJECTS_H_

#include "media/video/capture/screen/mouse_cursor_shape.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockScreenCapturer : public ScreenCapturer {
 public:
  MockScreenCapturer();
  virtual ~MockScreenCapturer();

  MOCK_METHOD1(Start, void(Callback* callback));
  MOCK_METHOD1(Capture, void(const webrtc::DesktopRegion& region));
  MOCK_METHOD1(SetMouseShapeObserver, void(
      MouseShapeObserver* mouse_shape_observer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScreenCapturer);
};

class MockScreenCapturerCallback : public ScreenCapturer::Callback {
 public:
  MockScreenCapturerCallback();
  virtual ~MockScreenCapturerCallback();

  MOCK_METHOD1(CreateSharedMemory, webrtc::SharedMemory*(size_t));
  MOCK_METHOD1(OnCaptureCompleted, void(webrtc::DesktopFrame*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScreenCapturerCallback);
};

class MockMouseShapeObserver : public ScreenCapturer::MouseShapeObserver {
 public:
  MockMouseShapeObserver();
  virtual ~MockMouseShapeObserver();

  void OnCursorShapeChanged(scoped_ptr<MouseCursorShape> cursor_shape) OVERRIDE;

  MOCK_METHOD1(OnCursorShapeChangedPtr,
               void(MouseCursorShape* cursor_shape));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMouseShapeObserver);

};


}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_MOCK_OBJECTS_H_
