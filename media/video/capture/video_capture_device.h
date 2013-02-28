// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureDevice is the abstract base class for realizing video capture
// device support in Chromium. It provides the interface for OS dependent
// implementations.
// The class is created and functions are invoked on a thread owned by
// VideoCaptureManager. Capturing is done on other threads depended on the OS
// specific implementation.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_

#include <list>
#include <string>

#include "base/time.h"
#include "media/base/media_export.h"
#include "media/video/capture/video_capture_types.h"

namespace media {

class MEDIA_EXPORT VideoCaptureDevice {
 public:

  struct Name {
    // Friendly name of a device
    std::string device_name;

    // Unique name of a device. Even if there are multiple devices with the same
    // friendly name connected to the computer this will be unique.
    std::string unique_id;
  };
  typedef std::list<Name> Names;

  class MEDIA_EXPORT EventHandler {
   public:
    // Captured a new video frame as a raw buffer. The size, color format, and
    // layout are taken from the parameters specified by an earlier call to
    // OnFrameInfo(). |data| must be packed, with no padding between rows and/or
    // color planes.
    virtual void OnIncomingCapturedFrame(const uint8* data,
                                         int length,
                                         base::Time timestamp,
                                         int rotation,  // Clockwise.
                                         bool flip_vert,
                                         bool flip_horiz) = 0;
    // Captured a new video frame, held in a VideoFrame container. |frame| must
    // be allocated as RGB32, YV12 or I420, and the size must match that
    // specified by an earlier call to OnFrameInfo().
    virtual void OnIncomingCapturedVideoFrame(media::VideoFrame* frame,
                                              base::Time timestamp) = 0;
    // An error has occurred that cannot be handled and VideoCaptureDevice must
    // be DeAllocate()-ed.
    virtual void OnError() = 0;
    // Called when VideoCaptureDevice::Allocate() has been called to inform of
    // the resulting frame size.
    virtual void OnFrameInfo(const VideoCaptureCapability& info) = 0;

   protected:
    virtual ~EventHandler() {}
  };
  // Creates a VideoCaptureDevice object.
  // Return NULL if the hardware is not available.
  static VideoCaptureDevice* Create(const Name& device_name);
  virtual ~VideoCaptureDevice() {}

  // Gets the names of all video capture devices connected to this computer.
  static void GetDeviceNames(Names* device_names);

  // Prepare the camera for use. After this function has been called no other
  // applications can use the camera. On completion EventHandler::OnFrameInfo()
  // is called informing of the resulting resolution and frame rate.
  // DeAllocate() must be called before this function can be called again and
  // before the object is deleted.
  virtual void Allocate(int width,
                        int height,
                        int frame_rate,
                        EventHandler* observer) = 0;

  // Start capturing video frames. Allocate must be called before this function.
  virtual void Start() = 0;

  // Stop capturing video frames.
  virtual void Stop() = 0;

  // Deallocates the camera. This means other applications can use it. After
  // this function has been called the capture device is reset to the state it
  // was when created.
  virtual void DeAllocate() = 0;

  // Get the name of the capture device.
  virtual const Name& device_name() = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_
