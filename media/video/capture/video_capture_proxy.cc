// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/video_frame.h"

namespace {

// Called on VC thread: extracts the state out of the VideoCapture, and
// serialize it into a VideoCaptureState.
media::VideoCaptureHandlerProxy::VideoCaptureState GetState(
    media::VideoCapture* capture) {
  media::VideoCaptureHandlerProxy::VideoCaptureState state;
  state.started = capture->CaptureStarted();
  state.frame_rate = capture->CaptureFrameRate();
  return state;
}

}  // anonymous namespace

namespace media {

VideoCaptureHandlerProxy::VideoCaptureHandlerProxy(
    VideoCapture::EventHandler* proxied,
    scoped_refptr<base::MessageLoopProxy> main_message_loop)
    : proxied_(proxied),
      main_message_loop_(main_message_loop) {
}

VideoCaptureHandlerProxy::~VideoCaptureHandlerProxy() {
}

void VideoCaptureHandlerProxy::OnStarted(VideoCapture* capture) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
        &VideoCaptureHandlerProxy::OnStartedOnMainThread,
        base::Unretained(this),
        capture,
        GetState(capture)));
}

void VideoCaptureHandlerProxy::OnStopped(VideoCapture* capture) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
        &VideoCaptureHandlerProxy::OnStoppedOnMainThread,
        base::Unretained(this),
        capture,
        GetState(capture)));
}

void VideoCaptureHandlerProxy::OnPaused(VideoCapture* capture) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoCaptureHandlerProxy::OnPausedOnMainThread,
      base::Unretained(this),
      capture,
      GetState(capture)));
}

void VideoCaptureHandlerProxy::OnError(VideoCapture* capture, int error_code) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoCaptureHandlerProxy::OnErrorOnMainThread,
      base::Unretained(this),
      capture,
      GetState(capture),
      error_code));
}

void VideoCaptureHandlerProxy::OnRemoved(VideoCapture* capture) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoCaptureHandlerProxy::OnRemovedOnMainThread,
      base::Unretained(this),
      capture,
      GetState(capture)));
}

void VideoCaptureHandlerProxy::OnFrameReady(
    VideoCapture* capture,
    const scoped_refptr<VideoFrame>& frame) {
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureHandlerProxy::OnFrameReadyOnMainThread,
                 base::Unretained(this),
                 capture,
                 GetState(capture),
                 frame));
}

void VideoCaptureHandlerProxy::OnStartedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state) {
  state_ = state;
  proxied_->OnStarted(capture);
}

void VideoCaptureHandlerProxy::OnStoppedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state) {
  state_ = state;
  proxied_->OnStopped(capture);
}

void VideoCaptureHandlerProxy::OnPausedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state) {
  state_ = state;
  proxied_->OnPaused(capture);
}

void VideoCaptureHandlerProxy::OnErrorOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state,
    int error_code) {
  state_ = state;
  proxied_->OnError(capture, error_code);
}

void VideoCaptureHandlerProxy::OnRemovedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state) {
  state_ = state;
  proxied_->OnRemoved(capture);
}

void VideoCaptureHandlerProxy::OnFrameReadyOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state,
    const scoped_refptr<VideoFrame>& frame) {
  state_ = state;
  proxied_->OnFrameReady(capture, frame);
}

}  // namespace media
