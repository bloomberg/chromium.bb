// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_module_impl.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "content/renderer/media/video_capture_impl_manager.h"

VideoCaptureModuleImpl::VideoCaptureModuleImpl(
    const media::VideoCaptureSessionId id,
    VideoCaptureImplManager* vc_manager)
    : webrtc::videocapturemodule::VideoCaptureImpl(id),
      session_id_(id),
      thread_("VideoCaptureModuleImpl"),
      stopped_event_(false, false),
      vc_manager_(vc_manager),
      state_(video_capture::kStopped),
      width_(-1),
      height_(-1),
      frame_rate_(-1),
      video_type_(webrtc::kVideoI420),
      capture_engine_(NULL),
      ref_count_(0) {
  DCHECK(vc_manager_);
  Init();
}

VideoCaptureModuleImpl::~VideoCaptureModuleImpl() {
  // Ensure to stop capture in case user doesn't do it.
  StopCapture();
  vc_manager_->RemoveDevice(session_id_, this);
  // Ensure capture stopped by |capture_engine_| since all tasks posted on this
  // thread will be executed and VCMImpl is waiting for OnStopped signal from
  // |capture_engine_|.
  thread_.Stop();
}

void VideoCaptureModuleImpl::Init() {
  thread_.Start();
  message_loop_proxy_ = thread_.message_loop_proxy();
  capture_engine_ = vc_manager_->AddDevice(session_id_, this);
}

int32_t VideoCaptureModuleImpl::AddRef() {
  DVLOG(1) << "VideoCaptureModuleImpl::AddRef()";
  return base::subtle::Barrier_AtomicIncrement(&ref_count_, 1);
}

int32_t VideoCaptureModuleImpl::Release() {
  DVLOG(1) << "VideoCaptureModuleImpl::Release()";
  int ret = base::subtle::Barrier_AtomicIncrement(&ref_count_, -1);
  if (ret == 0) {
    DVLOG(1) << "Reference count is zero, hence this object is now deleted.";
    delete this;
  }
  return ret;
}

WebRtc_Word32 VideoCaptureModuleImpl::StartCapture(
    const webrtc::VideoCaptureCapability& capability) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureModuleImpl::StartCaptureOnCaptureThread,
                 base::Unretained(this), capability));
  return 0;
}

WebRtc_Word32 VideoCaptureModuleImpl::StopCapture() {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureModuleImpl::StopCaptureOnCaptureThread,
                 base::Unretained(this)));
  return 0;
}

bool VideoCaptureModuleImpl::CaptureStarted() {
  return state_ == video_capture::kStarted;
}

WebRtc_Word32 VideoCaptureModuleImpl::CaptureSettings(
    webrtc::VideoCaptureCapability& settings) {
  settings.width = width_;
  settings.height = height_;
  settings.maxFPS = frame_rate_;
  settings.expectedCaptureDelay = 120;
  settings.rawType = webrtc::kVideoI420;
  return 0;
}

void VideoCaptureModuleImpl::OnStarted(media::VideoCapture* capture) {
  NOTIMPLEMENTED();
}

void VideoCaptureModuleImpl::OnStopped(media::VideoCapture* capture) {
  stopped_event_.Signal();
}

void VideoCaptureModuleImpl::OnPaused(media::VideoCapture* capture) {
  NOTIMPLEMENTED();
}

void VideoCaptureModuleImpl::OnError(media::VideoCapture* capture,
                                     int error_code) {
  NOTIMPLEMENTED();
}

void VideoCaptureModuleImpl::OnRemoved(media::VideoCapture* capture) {
  NOTIMPLEMENTED();
}

void VideoCaptureModuleImpl::OnBufferReady(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureModuleImpl::OnBufferReadyOnCaptureThread,
                 base::Unretained(this), capture, buf));
}

void VideoCaptureModuleImpl::OnDeviceInfoReceived(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  NOTIMPLEMENTED();
}

void VideoCaptureModuleImpl::StartCaptureOnCaptureThread(
    const webrtc::VideoCaptureCapability& capability) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_NE(state_, video_capture::kStarted);

  DVLOG(1) << "StartCaptureOnCaptureThread: " << capability.width << ", "
           << capability.height;

  StartCaptureInternal(capability);
  return;
}

void VideoCaptureModuleImpl::StartCaptureInternal(
    const webrtc::VideoCaptureCapability& capability) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(capability.rawType, webrtc::kVideoI420);

  video_type_ = capability.rawType;
  width_ = capability.width;
  height_ = capability.height;
  frame_rate_ = capability.maxFPS;
  state_ = video_capture::kStarted;

  media::VideoCapture::VideoCaptureCapability cap;
  cap.width = capability.width;
  cap.height = capability.height;
  cap.max_fps = capability.maxFPS;
  cap.raw_type = media::VideoFrame::I420;
  capture_engine_->StartCapture(this, cap);
}

void VideoCaptureModuleImpl::StopCaptureOnCaptureThread() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  if (state_ != video_capture::kStarted) {
    DVLOG(1) << "Got a StopCapture while not started!!! ";
    return;
  }

  DVLOG(1) << "StopCaptureOnCaptureThread. ";
  capture_engine_->StopCapture(this);
  // Need to use synchronous mode here to make sure |capture_engine_| will
  // not call VCMImpl after this function returns. Otherwise, there could be
  // use-after-free, especially when StopCapture() is called from dtor.
  stopped_event_.Wait();
  DVLOG(1) << "Capture Stopped!!! ";
  state_ = video_capture::kStopped;
  width_ = -1;
  height_ = -1;
  frame_rate_ = -1;
}

void VideoCaptureModuleImpl::OnBufferReadyOnCaptureThread(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  if (state_ != video_capture::kStarted)
    return;

  frameInfo_.width = buf->width;
  frameInfo_.height = buf->height;
  frameInfo_.rawType = video_type_;

  IncomingFrame(
      static_cast<WebRtc_UWord8*>(buf->memory_pointer),
      static_cast<WebRtc_Word32>(buf->buffer_size),
      frameInfo_,
      static_cast<WebRtc_Word64>(
          (buf->timestamp - start_time_).InMicroseconds()));

  capture->FeedBuffer(buf);
}
