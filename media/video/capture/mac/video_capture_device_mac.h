// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MacOSX implementation of generic VideoCaptureDevice, using either QTKit or
// AVFoundation as native capture API. QTKit is used in OSX versions 10.6 and
// previous, and AVFoundation is used in the rest.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

@protocol PlatformVideoCapturingMac;

namespace media {

// Called by VideoCaptureManager to open, close and start, stop video capture
// devices.
class VideoCaptureDeviceMac : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceMac(const Name& device_name);
  virtual ~VideoCaptureDeviceMac();

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(const VideoCaptureParams& params,
                                scoped_ptr<VideoCaptureDevice::Client> client)
      OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

  bool Init();

  // Called to deliver captured video frames.
  void ReceiveFrame(const uint8* video_frame,
                    int video_frame_length,
                    const VideoCaptureFormat& frame_format,
                    int aspect_numerator,
                    int aspect_denominator);

  void ReceiveError(const std::string& reason);

 private:
  void SetErrorState(const std::string& reason);
  bool UpdateCaptureResolution();

  // Flag indicating the internal state.
  enum InternalState {
    kNotInitialized,
    kIdle,
    kCapturing,
    kError
  };

  Name device_name_;
  scoped_ptr<VideoCaptureDevice::Client> client_;

  VideoCaptureFormat capture_format_;
  bool sent_frame_info_;
  bool tried_to_square_pixels_;

  // Only read and write state_ from inside this loop.
  const scoped_refptr<base::MessageLoopProxy> loop_proxy_;
  InternalState state_;

  // Used with Bind and PostTask to ensure that methods aren't called
  // after the VideoCaptureDeviceMac is destroyed.
  base::WeakPtrFactory<VideoCaptureDeviceMac> weak_factory_;
  base::WeakPtr<VideoCaptureDeviceMac> weak_this_;

  id<PlatformVideoCapturingMac> capture_device_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceMac);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
