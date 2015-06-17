// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/screen_capture_device_core.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"

namespace media {

namespace {

void DeleteCaptureMachine(
    scoped_ptr<VideoCaptureMachine> capture_machine) {
  capture_machine.reset();
}

}  // namespace

void ScreenCaptureDeviceCore::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kIdle) {
    DVLOG(1) << "Allocate() invoked when not in state Idle.";
    return;
  }

  if (params.requested_format.frame_rate <= 0) {
    std::string error_msg("Invalid frame_rate: ");
    error_msg += base::DoubleToString(params.requested_format.frame_rate);
    DVLOG(1) << error_msg;
    client->OnError(error_msg);
    return;
  }

  if (params.requested_format.pixel_format != PIXEL_FORMAT_I420 &&
      params.requested_format.pixel_format != PIXEL_FORMAT_TEXTURE) {
    std::string error_msg = base::StringPrintf(
        "unsupported format: %d", params.requested_format.pixel_format);
    DVLOG(1) << error_msg;
    client->OnError(error_msg);
    return;
  }

  if (params.requested_format.frame_size.IsEmpty()) {
    std::string error_msg =
        "invalid frame size: " + params.requested_format.frame_size.ToString();
    DVLOG(1) << error_msg;
    client->OnError(error_msg);
    return;
  }

  oracle_proxy_ = new ThreadSafeCaptureOracle(client.Pass(), params);

  capture_machine_->Start(
      oracle_proxy_,
      params,
      base::Bind(&ScreenCaptureDeviceCore::CaptureStarted, AsWeakPtr()));

  TransitionStateTo(kCapturing);
}

void ScreenCaptureDeviceCore::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kCapturing)
    return;

  oracle_proxy_->Stop();
  oracle_proxy_ = NULL;

  TransitionStateTo(kIdle);

  capture_machine_->Stop(base::Bind(&base::DoNothing));
}

void ScreenCaptureDeviceCore::CaptureStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!success) {
    std::string reason("Failed to start capture machine.");
    DVLOG(1) << reason;
    Error(reason);
  }
}

ScreenCaptureDeviceCore::ScreenCaptureDeviceCore(
    scoped_ptr<VideoCaptureMachine> capture_machine)
    : state_(kIdle),
      capture_machine_(capture_machine.Pass()) {
  DCHECK(capture_machine_.get());
}

ScreenCaptureDeviceCore::~ScreenCaptureDeviceCore() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(state_, kCapturing);
  if (capture_machine_) {
    capture_machine_->Stop(base::Bind(&DeleteCaptureMachine,
                           base::Passed(&capture_machine_)));
  }
  DVLOG(1) << "ScreenCaptureDeviceCore@" << this << " destroying.";
}

void ScreenCaptureDeviceCore::TransitionStateTo(State next_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

#ifndef NDEBUG
  static const char* kStateNames[] = {
    "Idle", "Allocated", "Capturing", "Error"
  };
  DVLOG(1) << "State change: " << kStateNames[state_]
           << " --> " << kStateNames[next_state];
#endif

  state_ = next_state;
}

void ScreenCaptureDeviceCore::Error(const std::string& reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kIdle)
    return;

  if (oracle_proxy_.get())
    oracle_proxy_->ReportError(reason);

  StopAndDeAllocate();
  TransitionStateTo(kError);
}

}  // namespace media
