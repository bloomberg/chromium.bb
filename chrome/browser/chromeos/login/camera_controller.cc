// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/camera_controller.h"

#include "base/compiler_specific.h"
#include "base/threading/thread_restrictions.h"

namespace chromeos {

namespace {

// Maximum number of capture failures we ignore before we try to initialize
// the camera again.
const int kMaxCaptureFailureCounter = 5;

// Maximum number of camera initialization retries.
const int kMaxCameraInitFailureCounter = 3;

// Name for camera thread.
const char kCameraThreadName[] = "Chrome_CameraThread";

}  // namespace

CameraController::CameraController(Delegate* delegate)
    : frame_width_(0),
      frame_height_(0),
      capture_failure_counter_(0),
      camera_init_failure_counter_(0),
      camera_thread_(new base::Thread(kCameraThreadName)),
      delegate_(delegate) {
  camera_thread_->Start();
}

CameraController::~CameraController() {
  if (camera_.get())
    camera_->set_delegate(NULL);
  {
    // A ScopedAllowIO object is required to join the thread when calling Stop.
    // See http://crosbug.com/11392.
    base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
    camera_thread_.reset();
  }
}

void CameraController::Start() {
  camera_ = new Camera(this, camera_thread_.get(), true);
  camera_->Initialize(frame_width_, frame_height_);
}

void CameraController::Stop() {
  if (!camera_.get())
    return;
  camera_->StopCapturing();
  camera_->Uninitialize();
}

void CameraController::GetFrame(SkBitmap* frame) const {
  if (camera_.get())
    camera_->GetFrame(frame);
}

void CameraController::OnInitializeSuccess() {
  DCHECK(camera_.get());
  camera_->StartCapturing();
}

void CameraController::OnInitializeFailure() {
  ++camera_init_failure_counter_;
  if (camera_init_failure_counter_ > kMaxCameraInitFailureCounter) {
    if (delegate_)
      delegate_->OnCaptureFailure();
  } else {
    // Retry initializing the camera.
    Start();
  }
}

void CameraController::OnStartCapturingSuccess() {
}

void CameraController::OnStartCapturingFailure() {
  // Try to reinitialize camera.
  OnInitializeFailure();
}

void CameraController::OnCaptureSuccess() {
  DCHECK(camera_.get());
  capture_failure_counter_ = 0;
  camera_init_failure_counter_ = 0;
  if (delegate_)
    delegate_->OnCaptureSuccess();
}

void CameraController::OnCaptureFailure() {
  ++capture_failure_counter_;
  if (capture_failure_counter_ < kMaxCaptureFailureCounter)
    return;

  capture_failure_counter_ = 0;
  OnInitializeFailure();
}

}  // namespace chromeos

