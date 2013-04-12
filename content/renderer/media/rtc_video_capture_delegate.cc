// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_capture_delegate.h"

#include "base/bind.h"

namespace content {

RtcVideoCaptureDelegate::RtcVideoCaptureDelegate(
    const media::VideoCaptureSessionId id,
    VideoCaptureImplManager* vc_manager)
    : session_id_(id),
      vc_manager_(vc_manager),
      capture_engine_(NULL),
      got_first_frame_(false),
      error_occured_(false) {
  DVLOG(3) << " RtcVideoCaptureDelegate::ctor";
  capture_engine_ = vc_manager_->AddDevice(session_id_, this);
}

RtcVideoCaptureDelegate::~RtcVideoCaptureDelegate() {
  DVLOG(3) << " RtcVideoCaptureDelegate::dtor";
  vc_manager_->RemoveDevice(session_id_, this);
}

void RtcVideoCaptureDelegate::StartCapture(
    const media::VideoCaptureCapability& capability,
    const FrameCapturedCallback& captured_callback,
    const StateChangeCallback& state_callback) {
  DVLOG(3) << " RtcVideoCaptureDelegate::StartCapture ";
  message_loop_proxy_ = base::MessageLoopProxy::current();
  captured_callback_ = captured_callback;
  state_callback_ = state_callback;
  got_first_frame_ = false;
  error_occured_ = false;

  // Increase the reference count to ensure we are not deleted until
  // The we are unregistered in RtcVideoCaptureDelegate::OnRemoved.
  AddRef();
  capture_engine_->StartCapture(this, capability);
}

void RtcVideoCaptureDelegate::StopCapture() {
  // Immediately make sure we don't provide more frames.
  captured_callback_.Reset();
  state_callback_.Reset();
  capture_engine_->StopCapture(this);
}

void RtcVideoCaptureDelegate::OnStarted(media::VideoCapture* capture) {
  DVLOG(3) << " RtcVideoCaptureDelegate::OnStarted";
}

void RtcVideoCaptureDelegate::OnStopped(media::VideoCapture* capture) {
}

void RtcVideoCaptureDelegate::OnPaused(media::VideoCapture* capture) {
  NOTIMPLEMENTED();
}

void RtcVideoCaptureDelegate::OnError(media::VideoCapture* capture,
                                      int error_code) {
  DVLOG(3) << " RtcVideoCaptureDelegate::OnError";
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&RtcVideoCaptureDelegate::OnErrorOnCaptureThread,
                 this, capture));
}

void RtcVideoCaptureDelegate::OnRemoved(media::VideoCapture* capture) {
  DVLOG(3) << " RtcVideoCaptureDelegate::OnRemoved";
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&RtcVideoCaptureDelegate::OnRemovedOnCaptureThread,
                 this, capture));

  // Balance the AddRef in StartCapture.
  // This means we are no longer registered as an event handler and can safely
  // be deleted.
  Release();
}

void RtcVideoCaptureDelegate::OnBufferReady(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&RtcVideoCaptureDelegate::OnBufferReadyOnCaptureThread,
                 this, capture, buf));
}

void RtcVideoCaptureDelegate::OnDeviceInfoReceived(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  NOTIMPLEMENTED();
}

void RtcVideoCaptureDelegate::OnBufferReadyOnCaptureThread(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  if (!captured_callback_.is_null()) {
    if (!got_first_frame_) {
      got_first_frame_ = true;
      if (!state_callback_.is_null())
        state_callback_.Run(CAPTURE_RUNNING);
    }

    captured_callback_.Run(*buf);
  }
  capture->FeedBuffer(buf);
}

void RtcVideoCaptureDelegate::OnErrorOnCaptureThread(
    media::VideoCapture* capture) {
  error_occured_ = true;
  if (!state_callback_.is_null())
    state_callback_.Run(CAPTURE_FAILED);
}


void RtcVideoCaptureDelegate::OnRemovedOnCaptureThread(
    media::VideoCapture* capture) {
  if (!error_occured_ && !state_callback_.is_null())
    state_callback_.Run(CAPTURE_STOPPED);
}

}  // namespace content
