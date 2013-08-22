// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OS X implementation of VideoCaptureDevice, using QTKit as native capture API.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

@class VideoCaptureDeviceQTKit;

namespace media {

// Called by VideoCaptureManager to open, close and start, stop video capture
// devices.
class VideoCaptureDeviceMac : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceMac(const Name& device_name);
  virtual ~VideoCaptureDeviceMac();

  // VideoCaptureDevice implementation.
  virtual void Allocate(const VideoCaptureCapability& capture_format,
                         VideoCaptureDevice::EventHandler* observer) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void DeAllocate() OVERRIDE;
  virtual const Name& device_name() OVERRIDE;

  bool Init();

  // Called to deliver captured video frames.
  void ReceiveFrame(const uint8* video_frame, int video_frame_length,
                    const VideoCaptureCapability& frame_info);

  void ReceiveError(const std::string& reason);

 private:
  void SetErrorState(const std::string& reason);

  // Flag indicating the internal state.
  enum InternalState {
    kNotInitialized,
    kIdle,
    kAllocated,
    kCapturing,
    kError
  };

  Name device_name_;
  VideoCaptureDevice::EventHandler* observer_;

  // Only read and write state_ from inside this loop.
  const scoped_refptr<base::MessageLoopProxy> loop_proxy_;
  InternalState state_;

  // Used with Bind and PostTask to ensure that methods aren't called
  // after the VideoCaptureDeviceMac is destroyed.
  base::WeakPtrFactory<VideoCaptureDeviceMac> weak_factory_;
  base::WeakPtr<VideoCaptureDeviceMac> weak_this_;

  VideoCaptureDeviceQTKit* capture_device_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceMac);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
