// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_capture_delegate.h"

#include "base/bind.h"

RtcVideoCaptureDelegate::RtcVideoCaptureDelegate(
    const media::VideoCaptureSessionId id,
    VideoCaptureImplManager* vc_manager)
    : session_id_(id),
      vc_manager_(vc_manager),
      capture_engine_(NULL) {
  DVLOG(3) << " RtcVideoCaptureDelegate::ctor";
  capture_engine_ = vc_manager_->AddDevice(session_id_, this);
}

RtcVideoCaptureDelegate::~RtcVideoCaptureDelegate() {
  DVLOG(3) << " RtcVideoCaptureDelegate::dtor";
  vc_manager_->RemoveDevice(session_id_, this);
}

void RtcVideoCaptureDelegate::StartCapture(
    const media::VideoCaptureCapability& capability,
    const FrameCapturedCallback& callback) {
  DVLOG(3) << " RtcVideoCaptureDelegate::StartCapture ";
  message_loop_proxy_ = base::MessageLoopProxy::current();
  captured_callback_ = callback;

  // Increase the reference count to ensure we are not deleted until
  // The we are unregistered in RtcVideoCaptureDelegate::OnRemoved.
  AddRef();
  capture_engine_->StartCapture(this, capability);
}

void RtcVideoCaptureDelegate::StopCapture() {
  // Immediately make sure we don't provide more frames.
  captured_callback_.Reset();
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
  NOTIMPLEMENTED();
}

void RtcVideoCaptureDelegate::OnRemoved(media::VideoCapture* capture) {
  DVLOG(3) << " RtcVideoCaptureDelegate::OnRemoved";
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
    captured_callback_.Run(*buf);
  }
  capture->FeedBuffer(buf);
}

