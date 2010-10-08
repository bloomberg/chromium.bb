// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CAMERA_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CAMERA_H_
#pragma once

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/timer.h"

class SkBitmap;

namespace base {
class TimeDelta;
}  // namespace base

namespace chromeos {

// Class that wraps interaction with video capturing device. Returns
// frames captured with specified intervals of time via delegate interface.
// All communication with camera driver is performed on IO thread.
// Delegate's callback are called on UI thread.
class Camera : public base::RefCountedThreadSafe<Camera> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Callbacks that notify of the initialization status.
    virtual void OnInitializeSuccess() = 0;
    virtual void OnInitializeFailure() = 0;

    // Callbacks that notify if video capturing was started successfully or
    // not.
    virtual void OnStartCapturingSuccess() = 0;
    virtual void OnStartCapturingFailure() = 0;

    // Called if video frame was captured successfully.
    virtual void OnCaptureSuccess(const SkBitmap& frame) = 0;
    // Called if capturing the current frame failed.
    virtual void OnCaptureFailure() = 0;
  };

  // Initializes object members. |delegate| is object that will receive
  // notifications about success of async method calls. |mirrored|
  // determines if the returned video image is mirrored horizontally.
  Camera(Delegate* delegate, bool mirrored);

  // Initializes camera device on IO thread. Corresponding delegate's callback
  // is called on UI thread to notify about success or failure. Does nothing if
  // camera is successfully initialized already. Sets the desired width and
  // height of the frame to receive from camera.
  void Initialize(int desired_width, int desired_height);

  // Uninitializes the camera on IO thread. Can be called anytime, any number
  // of times.
  void Uninitialize();

  // Starts capturing video frames with specified interval, in ms. Calls the
  // corresponding method of delegate to report about success or failure. If
  // succeeded, subsequent call doesn't do anything.
  void StartCapturing(int64 rate);

  // Stops capturing video frames. Can be called anytime, any number of
  // times.
  void StopCapturing();

  // Setter for delegate: allows to set it to NULL when delegate is about to
  // be destroyed.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  // Destructor is private so only its base class can delete Camera objects.
  ~Camera();
  friend class base::RefCountedThreadSafe<Camera>;

  // Tries to open the device with specified name. Returns opened device
  // descriptor if succeeds, -1 otherwise.
  int OpenDevice(const char* device_name) const;

  // Initializes reading mode for the device. Returns true on success, false
  // otherwise.
  bool InitializeReadingMode(int fd);

  // Unmaps video buffers stored in |buffers_|.
  void UnmapVideoBuffers();

  // Task for IO thread that queries camera about the next frame and sends it to
  // |delegate_| via its method or reports failure. Schedules the next task
  // for itself if capturing still takes place.
  void OnCapture();

  // Reads a frame from the video device. If retry is needed, returns false.
  // Otherwise, returns true despite of success status.
  bool ReadFrame();

  // Transforms raw data received from camera into SkBitmap with desired
  // size and notifies the delegate that the image is ready.
  void ProcessImage(void* data);

  // Actual routines that run on IO thread and call delegate's callbacks. See
  // the corresponding methods without Do prefix for details.
  void DoInitialize(int desired_width, int desired_height);
  void DoStartCapturing(int64 rate);

  // Helper method that reports failure to the delegate via method
  // corresponding to the current state of the object.
  void ReportFailure();

  // Methods called on UI thread to call delegate.
  void OnInitializeSuccess();
  void OnInitializeFailure();
  void OnStartCapturingSuccess();
  void OnStartCapturingFailure();
  void OnCaptureSuccess(const SkBitmap& frame);
  void OnCaptureFailure();

  // IO thread routines that implement the corresponding public methods.
  void DoUninitialize();
  void DoStopCapturing();

  // Defines a buffer in memory where one frame from the camera is stored.
  struct VideoBuffer {
    void* start;
    size_t length;
  };

  // Delegate that receives the frames from the camera.
  // Delegate is accessed only on UI thread.
  Delegate* delegate_;

  // All the members below are accessed only on IO thread.
  // Name of the device file, i.e. "/dev/video0".
  std::string device_name_;

  // File descriptor of the opened device.
  int device_descriptor_;

  // Vector of buffers where to store video frames from camera.
  std::vector<VideoBuffer> buffers_;

  // Indicates if capturing has been started.
  bool is_capturing_;

  // Rate at which camera device should be queried about new image, in ms.
  int64 capturing_rate_;

  // Desired size of the frame to get from camera. If it doesn't match
  // camera's supported resolution, higher resolution is selected (if
  // available) and frame is cropped. If higher resolution is not available,
  // the highest is selected and resized.
  int desired_width_;
  int desired_height_;

  // Size of the frame that camera will give to us. It may not match the
  // desired size.
  int frame_width_;
  int frame_height_;

  // If set to true, the returned image will be reflected from the Y axis to
  // mimic mirror behavior.
  bool mirrored_;

  DISALLOW_COPY_AND_ASSIGN(Camera);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CAMERA_H_
