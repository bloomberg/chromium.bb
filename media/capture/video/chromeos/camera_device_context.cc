// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_device_context.h"

namespace media {

CameraDeviceContext::CameraDeviceContext(
    std::unique_ptr<VideoCaptureDevice::Client> client)
    : state_(State::kStopped), rotation_(0), client_(std::move(client)) {
  DCHECK(client_);
}

CameraDeviceContext::~CameraDeviceContext() {}

void CameraDeviceContext::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = state;
  if (state_ == State::kCapturing) {
    client_->OnStarted();
  }
}

CameraDeviceContext::State CameraDeviceContext::GetState() {
  return state_;
}

void CameraDeviceContext::SetErrorState(
    const tracked_objects::Location& from_here,
    const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kError;
  LOG(ERROR) << reason;
  client_->OnError(from_here, reason);
}

void CameraDeviceContext::LogToClient(std::string message) {
  client_->OnLog(message);
}

void CameraDeviceContext::SubmitCapturedData(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  client_->OnIncomingCapturedData(data, length, frame_format, rotation_,
                                  reference_time, timestamp);
}

void CameraDeviceContext::SetRotation(int rotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(rotation >= 0 && rotation < 360 && rotation % 90 == 0);
  rotation_ = rotation;
}

}  // namespace media
