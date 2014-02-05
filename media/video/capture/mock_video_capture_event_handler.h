// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_EVENT_HANDLER_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_EVENT_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/video/capture/video_capture.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockVideoCaptureEventHandler : public VideoCapture::EventHandler {
 public:
  MockVideoCaptureEventHandler();
  virtual ~MockVideoCaptureEventHandler();

  // EventHandler implementation.
  MOCK_METHOD1(OnStarted, void(VideoCapture* capture));
  MOCK_METHOD1(OnStopped, void(VideoCapture* capture));
  MOCK_METHOD1(OnPaused, void(VideoCapture* capture));
  MOCK_METHOD2(OnError, void(VideoCapture* capture, int error_code));
  MOCK_METHOD1(OnRemoved, void(VideoCapture* capture));
  MOCK_METHOD2(OnFrameReady,
               void(VideoCapture* capture,
                    const scoped_refptr<VideoFrame>& frame));
  MOCK_METHOD2(OnDeviceInfoReceived,
               void(VideoCapture* capture,
                    const VideoCaptureFormat& device_info));
  MOCK_METHOD1(OnDeviceSupportedFormatsEnumerated,
               void(const media::VideoCaptureFormats& supported_formats));
  MOCK_METHOD1(OnDeviceFormatsInUseReceived,
               void(const media::VideoCaptureFormats& formats_in_use));
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_EVENT_HANDLER_H_
