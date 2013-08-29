// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gl_surface_capturer.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_message_macros.h"

namespace content {

namespace {

enum {
  kMaxCaptureSize = 4096,
};

}  // anonymous namespace

GLSurfaceCapturer::GLSurfaceCapturer(int32 route_id, GpuCommandBufferStub* stub)
    : thread_checker_(),
      route_id_(route_id),
      stub_(stub),
      output_format_(media::VideoFrame::INVALID) {
  DCHECK(thread_checker_.CalledOnValidThread());
  stub_->AddDestructionObserver(this);
  stub_->channel()->AddRoute(route_id_, this);
}

GLSurfaceCapturer::~GLSurfaceCapturer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (surface_capturer_)
    surface_capturer_.release()->Destroy();

  stub_->channel()->RemoveRoute(route_id_);
  stub_->RemoveDestructionObserver(this);
}

bool GLSurfaceCapturer::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GLSurfaceCapturer, message)
    IPC_MESSAGE_HANDLER(SurfaceCapturerMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(SurfaceCapturerMsg_TryCapture, OnTryCapture)
    IPC_MESSAGE_HANDLER(SurfaceCapturerMsg_CopyCaptureToVideoFrame,
                        OnCopyCaptureToVideoFrame)
    IPC_MESSAGE_HANDLER(SurfaceCapturerMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GLSurfaceCapturer::NotifyCaptureParameters(const gfx::Size& buffer_size,
                                                const gfx::Rect& visible_rect) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (buffer_size.width() < 1 || buffer_size.width() > kMaxCaptureSize ||
      buffer_size.height() < 1 || buffer_size.height() > kMaxCaptureSize ||
      visible_rect.x() < 0 || visible_rect.x() > kMaxCaptureSize - 1 ||
      visible_rect.y() < 0 || visible_rect.y() > kMaxCaptureSize - 1 ||
      visible_rect.width() < 1 || visible_rect.width() > kMaxCaptureSize ||
      visible_rect.height() < 1 || visible_rect.height() > kMaxCaptureSize) {
    DLOG(ERROR) << "NotifyCaptureParameters(): parameters out of bounds: "
                   "buffer_size=" << buffer_size.ToString()
                << ", visible_rect=" << visible_rect.ToString();
    NotifyError(SurfaceCapturer::kInvalidArgumentError);
    return;
  }
  output_buffer_size_ = buffer_size;
  output_visible_rect_ = visible_rect;
  Send(new SurfaceCapturerHostMsg_NotifyCaptureParameters(
      route_id_, buffer_size, visible_rect));
}

void GLSurfaceCapturer::NotifyCopyCaptureDone(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FrameIdMap::iterator iter = frame_id_map_.find(frame);
  if (iter == frame_id_map_.end()) {
    DLOG(ERROR) << "NotifyCopyCaptureDone(): invalid frame";
    NotifyError(SurfaceCapturer::kInvalidArgumentError);
    return;
  }
  Send(new SurfaceCapturerHostMsg_NotifyCopyCaptureDone(route_id_,
                                                        iter->second));
  frame_id_map_.erase(iter);
}

void GLSurfaceCapturer::NotifyError(SurfaceCapturer::Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Send(new SurfaceCapturerHostMsg_NotifyError(route_id_, error));
}

void GLSurfaceCapturer::OnWillDestroyStub() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete this;
}

void GLSurfaceCapturer::OnInitialize(media::VideoFrame::Format format) {
  DVLOG(2) << "OnInitialize(): format=" << format;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!surface_capturer_);

  // TODO(sheu): actually create a surface capturer.

  if (!surface_capturer_) {
    NOTIMPLEMENTED() << "OnInitialize(): GL surface capturing not available";
    NotifyError(SurfaceCapturer::kPlatformFailureError);
    return;
  }

  output_format_ = format;
  surface_capturer_->Initialize(output_format_);
}

void GLSurfaceCapturer::OnTryCapture() {
  DVLOG(3) << "OnTryCapture()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!surface_capturer_)
    return;
  surface_capturer_->TryCapture();
}

static void DeleteShm(scoped_ptr<base::SharedMemory> shm) {
  // Just let shm fall out of scope.
}

void GLSurfaceCapturer::OnCopyCaptureToVideoFrame(
    int32 frame_id,
    base::SharedMemoryHandle buffer_shm,
    uint32_t buffer_size) {
  DVLOG(3) << "OnCopyCaptureToVideoFrame(): frame_id=" << frame_id
           << ", buffer_size=" << buffer_size;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!surface_capturer_)
    return;
  if (frame_id < 0) {
    DLOG(ERROR) << "OnCopyCaptureToVideoFrame(): invalid frame_id=" << frame_id;
    NotifyError(SurfaceCapturer::kPlatformFailureError);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory(buffer_shm, false));
  if (!shm->Map(buffer_size)) {
    DLOG(ERROR) << "OnCopyCaptureToVideoFrame(): could not map "
                   "frame_id=" << frame_id;
    NotifyError(SurfaceCapturer::kPlatformFailureError);
    return;
  }
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalSharedMemory(
          output_format_,
          output_buffer_size_,
          output_visible_rect_,
          output_visible_rect_.size(),
          reinterpret_cast<uint8*>(shm->memory()),
          buffer_size,
          buffer_shm,
          base::TimeDelta(),
          base::Bind(&DeleteShm, base::Passed(&shm)));
  if (!frame) {
    DLOG(ERROR) << "OnCopyCaptureToVideoFrame(): could not create frame for"
                   "frame_id=" << frame_id;
    NotifyError(SurfaceCapturer::kPlatformFailureError);
    return;
  }
  frame_id_map_[frame] = frame_id;
  surface_capturer_->CopyCaptureToVideoFrame(frame);
}

void GLSurfaceCapturer::OnDestroy() {
  DVLOG(2) << "OnDestroy()";
  DCHECK(thread_checker_.CalledOnValidThread());
  delete this;
}

void GLSurfaceCapturer::Send(IPC::Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  uint32 type = message->type();
  if (!stub_->channel()->Send(message)) {
    DLOG(ERROR) << "Send(): send failed: message->type()=" << type;
    NotifyError(SurfaceCapturer::kPlatformFailureError);
  }
}

}  // namespace content
