// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gl_surface_capturer_host.h"

#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_message_macros.h"

namespace content {

namespace {

enum {
  kNumCaptureBuffers = 2,
  kMaxCaptureSize = 4096,
};

}  // anonymous namespace

GLSurfaceCapturerHost::GLSurfaceCapturerHost(int32 capturer_route_id,
                                             SurfaceCapturer::Client* client,
                                             CommandBufferProxyImpl* impl)
    : thread_checker_(),
      capturer_route_id_(capturer_route_id),
      weak_this_factory_(this),
      client_(client),
      impl_(impl),
      channel_(impl_->channel()),
      next_frame_id_(0) {
  DCHECK(thread_checker_.CalledOnValidThread());
  channel_->AddRoute(capturer_route_id_, weak_this_factory_.GetWeakPtr());
  impl_->AddDeletionObserver(this);
}

void GLSurfaceCapturerHost::OnChannelError() {
  DLOG(ERROR) << "OnChannelError()";
  DCHECK(thread_checker_.CalledOnValidThread());
  OnNotifyError(kPlatformFailureError);
  if (channel_) {
    weak_this_factory_.InvalidateWeakPtrs();
    channel_->RemoveRoute(capturer_route_id_);
    channel_ = NULL;
  }
}

bool GLSurfaceCapturerHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GLSurfaceCapturerHost, message)
    IPC_MESSAGE_HANDLER(SurfaceCapturerHostMsg_NotifyCaptureParameters,
                        OnNotifyCaptureParameters)
    IPC_MESSAGE_HANDLER(SurfaceCapturerHostMsg_NotifyCopyCaptureDone,
                        OnNotifyCopyCaptureDone)
    IPC_MESSAGE_HANDLER(SurfaceCapturerHostMsg_NotifyError, OnNotifyError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GLSurfaceCapturerHost::Initialize(media::VideoFrame::Format format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Send(new SurfaceCapturerMsg_Initialize(capturer_route_id_, format));
}

void GLSurfaceCapturerHost::TryCapture() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Send(new SurfaceCapturerMsg_TryCapture(capturer_route_id_));
}

void GLSurfaceCapturerHost::CopyCaptureToVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!channel_)
    return;
  if (!base::SharedMemory::IsHandleValid(frame->shared_memory_handle())) {
    DLOG(ERROR) << "CopyCaptureToVideoFrame(): cannot capture to frame not "
                   "backed by shared memory";
    OnNotifyError(kPlatformFailureError);
    return;
  }
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(frame->shared_memory_handle());
  if (!base::SharedMemory::IsHandleValid(frame->shared_memory_handle())) {
    DLOG(ERROR) << "CopyCaptureToVideoFrame(): failed to duplicate buffer "
                   "handle for GPU process";
    OnNotifyError(kPlatformFailureError);
    return;
  }

  const size_t plane_count = media::VideoFrame::NumPlanes(frame->format());
  size_t frame_size = 0;
  for (size_t i = 0; i < plane_count; ++i) {
    DCHECK_EQ(reinterpret_cast<void*>(frame->data(i)),
              reinterpret_cast<void*>((frame->data(0) + frame_size)))
        << "plane=" << i;
    frame_size += frame->stride(i) * frame->rows(i);
  }

  Send(new SurfaceCapturerMsg_CopyCaptureToVideoFrame(
      capturer_route_id_, next_frame_id_, handle, frame_size));
  frame_map_[next_frame_id_] = frame;
  next_frame_id_ = (next_frame_id_ + 1) & 0x3FFFFFFF;
}

void GLSurfaceCapturerHost::Destroy() {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_ = NULL;
  Send(new SurfaceCapturerMsg_Destroy(capturer_route_id_));
  delete this;
}

void GLSurfaceCapturerHost::OnWillDeleteImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_ = NULL;
  OnChannelError();
}

GLSurfaceCapturerHost::~GLSurfaceCapturerHost() {
  DCHECK(thread_checker_.CalledOnValidThread());
  weak_this_factory_.InvalidateWeakPtrs();
  if (channel_)
    channel_->RemoveRoute(capturer_route_id_);
  if (impl_)
    impl_->RemoveDeletionObserver(this);
}

void GLSurfaceCapturerHost::NotifyError(Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&GLSurfaceCapturerHost::OnNotifyError,
                 weak_this_factory_.GetWeakPtr(),
                 error));
}

void GLSurfaceCapturerHost::OnNotifyCaptureParameters(
    const gfx::Size& buffer_size,
    const gfx::Rect& visible_rect) {
  DVLOG(2) << "OnNotifyCaptureParameters(): "
              "buffer_size=" << buffer_size.ToString()
           << ", visible_rect=" << visible_rect.ToString();
  DCHECK(thread_checker_.CalledOnValidThread());
  if (buffer_size.width() < 1 || buffer_size.width() > kMaxCaptureSize ||
      buffer_size.height() < 1 || buffer_size.height() > kMaxCaptureSize ||
      visible_rect.x() < 0 || visible_rect.x() > kMaxCaptureSize - 1 ||
      visible_rect.y() < 0 || visible_rect.y() > kMaxCaptureSize - 1 ||
      visible_rect.width() < 1 || visible_rect.width() > kMaxCaptureSize ||
      visible_rect.height() < 1 || visible_rect.height() > kMaxCaptureSize) {
    DLOG(ERROR) << "OnNotifyCaptureParameters(): parameters out of bounds: "
                   "buffer_size=" << buffer_size.ToString()
                << ", visible_rect=" << visible_rect.ToString();
    OnNotifyError(kPlatformFailureError);
    return;
  }
  if (client_)
    client_->NotifyCaptureParameters(buffer_size, visible_rect);
}

void GLSurfaceCapturerHost::OnNotifyCopyCaptureDone(int32 frame_id) {
  DVLOG(3) << "OnNotifyCopyCaptureDone(): frame_id=" << frame_id;
  DCHECK(thread_checker_.CalledOnValidThread());
  FrameMap::iterator iter = frame_map_.find(frame_id);
  if (iter == frame_map_.end()) {
    DLOG(ERROR) << "OnNotifyCopyCaptureDone(): invalid frame_id=" << frame_id;
    OnNotifyError(kPlatformFailureError);
    return;
  }
  if (client_)
    client_->NotifyCopyCaptureDone(iter->second);
  frame_map_.erase(iter);
}

void GLSurfaceCapturerHost::OnNotifyError(Error error) {
  DVLOG(2) << "OnNotifyError(): error=" << error;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (client_) {
    client_->NotifyError(error);
    client_ = NULL;
  }
}

void GLSurfaceCapturerHost::Send(IPC::Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  uint32 type = message->type();
  if (!channel_) {
    DLOG(ERROR) << "Send(): no channel";
    delete message;
    NotifyError(kPlatformFailureError);
  } else if (!channel_->Send(message)) {
    DLOG(ERROR) << "Send(): failed: message->type()=" << type;
    NotifyError(kPlatformFailureError);
  }
}

}  // namespace content
