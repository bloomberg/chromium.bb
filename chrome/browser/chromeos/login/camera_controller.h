// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CAMERA_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CAMERA_CONTROLLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/login/camera.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chromeos {

class CameraController: public Camera::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when new frame was captured by camera.
    virtual void OnCaptureSuccess() = 0;

    // Called when camera failed to initialize or get the next frame.
    virtual void OnCaptureFailure() = 0;
  };

  explicit CameraController(Delegate* delegate);
  virtual ~CameraController();

  void set_frame_width(int width) { frame_width_ = width; }
  int frame_width() const { return frame_width_; }

  void set_frame_height(int height) { frame_height_ = height; }
  int frame_height() const { return frame_height_; }

  // Initializes camera and starts video capturing.
  void Start();

  // Stops video capturing and deinitializes camera.
  void Stop();

  // Returns the last captured frame from the camera.
  void GetFrame(SkBitmap* frame) const;

  // Camera::Delegate implementation:
  virtual void OnInitializeSuccess() OVERRIDE;
  virtual void OnInitializeFailure() OVERRIDE;
  virtual void OnStartCapturingSuccess() OVERRIDE;
  virtual void OnStartCapturingFailure() OVERRIDE;
  virtual void OnCaptureSuccess() OVERRIDE;
  virtual void OnCaptureFailure() OVERRIDE;

 private:
  // Size of frame we want to receive.
  int frame_width_;
  int frame_height_;

  // Object that handles video capturing.
  scoped_refptr<Camera> camera_;

  // Counts how many times in a row capture failed.
  int capture_failure_counter_;

  // Counts how many times camera initialization failed.
  int camera_init_failure_counter_;

  // Thread for camera to work on.
  scoped_ptr<base::Thread> camera_thread_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CameraController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CAMERA_CONTROLLER_H_
