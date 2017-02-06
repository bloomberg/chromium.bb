// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_H_

#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

// Callback interface for VideoCaptureDeviceClient to communicate with its
// clients.
class CAPTURE_EXPORT VideoFrameReceiver {
 public:
  virtual ~VideoFrameReceiver(){};

  virtual void OnIncomingCapturedVideoFrame(
      media::VideoCaptureDevice::Client::Buffer buffer,
      scoped_refptr<media::VideoFrame> frame) = 0;
  virtual void OnError() = 0;
  virtual void OnLog(const std::string& message) = 0;

  // Tells the VideoFrameReceiver that the producer is no longer going to use
  // the buffer with id |buffer_id| for frame delivery. This may be called even
  // while the receiver is still consuming the buffer from a call to
  // OnIncomingCapturedVideoFrame(). In that case, it means that the
  // caller is asking the VideoFrameReceiver to release the buffer
  // at its earliest convenience.
  // A producer may reuse a retired |buffer_id| immediately after this call.
  virtual void OnBufferRetired(int buffer_id) = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_H_
