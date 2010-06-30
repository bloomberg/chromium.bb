// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CAMERA_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CAMERA_H_

#include <string>
#include <vector>

#include "base/timer.h"

class SkBitmap;

namespace base {
class TimeDelta;
}  // namespace base

namespace chromeos {

// Class that wraps interaction with video capturing device. Returns
// frames captured with specified intervals of time via delegate interface.
class Camera {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called with specified intervals with decoded frame as a parameter.
    virtual void OnVideoFrameCaptured(const SkBitmap& frame) = 0;
  };

  explicit Camera(Delegate* delegate);
  ~Camera();

  // Initializes camera device. Returns true if succeeded, false otherwise.
  // Does nothing on subsequent calls until Uninitialize is called.
  // Sets the desired width and height of the frame to receive from camera.
  bool Initialize(int desired_width, int desired_height);

  // Uninitializes the camera. Can be called anytime, any number of times.
  void Uninitialize();

  // Starts capturing video frames with specified interval.
  // Does nothing on subsequent calls until StopCapturing is called.
  // Returns true if succeeded, false otherwise.
  bool StartCapturing(const base::TimeDelta& interval);

  // Stops capturing video frames. Can be called anytime, any number of
  // times.
  void StopCapturing();

  void set_mirrored(bool mirrored) { mirrored_ = mirrored; }

 private:
  // Tries to open the device with specified name. Returns opened device
  // descriptor if succeeds, -1 otherwise.
  int OpenDevice(const char* device_name) const;

  // Initializes reading mode for the device. Returns true on success, false
  // otherwise.
  bool InitializeReadingMode(int fd);

  // Unmaps video buffers stored in |buffers_|.
  void UnmapVideoBuffers();

  // Called by |timer_| to get the frame from video device and send it to
  // |delegate_| via its method.
  void OnCapture();

  // Reads a frame from the video device. If retry is needed, returns false.
  // Otherwise, returns true despite of success status.
  bool ReadFrame();

  // Transforms raw data received from camera into SkBitmap with desired
  // size and notifies the delegate that the image is ready.
  void ProcessImage(void* data);

  // Defines a buffer in memory where one frame from the camera is stored.
  struct VideoBuffer {
    void* start;
    size_t length;
  };

  // Delegate that receives the frames from the camera.
  Delegate* delegate_;

  // Name of the device file, i.e. "/dev/video0".
  std::string device_name_;

  // File descriptor of the opened device.
  int device_descriptor_;

  // Vector of buffers where to store video frames from camera.
  std::vector<VideoBuffer> buffers_;

  // Timer for getting frames.
  base::RepeatingTimer<Camera> timer_;

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
