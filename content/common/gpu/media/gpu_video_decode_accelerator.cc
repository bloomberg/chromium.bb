// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gpu_video_decode_accelerator.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif  // OS_WIN

#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#if (defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)) || defined(OS_WIN)
#if defined(OS_WIN)
#include "content/common/gpu/media/dxva_video_decode_accelerator.h"
#else  // OS_WIN
#include "content/common/gpu/media/omx_video_decode_accelerator.h"
#endif  // OS_WIN
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#endif

#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/size.h"

using gpu::gles2::TextureManager;

GpuVideoDecodeAccelerator::GpuVideoDecodeAccelerator(
    IPC::Message::Sender* sender,
    int32 host_route_id,
    GpuCommandBufferStub* stub)
    : sender_(sender),
      init_done_msg_(NULL),
      host_route_id_(host_route_id),
      stub_(stub),
      video_decode_accelerator_(NULL) {
}

GpuVideoDecodeAccelerator::~GpuVideoDecodeAccelerator() {
  if (video_decode_accelerator_)
    video_decode_accelerator_->Destroy();
}

bool GpuVideoDecodeAccelerator::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAccelerator, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Decode, OnDecode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_AssignPictureBuffers,
                        OnAssignPictureBuffers)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                        OnReusePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Flush, OnFlush)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32 requested_num_of_buffers, const gfx::Size& dimensions) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers(
          host_route_id_, requested_num_of_buffers, dimensions))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers) "
                << "failed";
  }
}

void GpuVideoDecodeAccelerator::DismissPictureBuffer(
    int32 picture_buffer_id) {
  // Notify client that picture buffer is now unused.
  if (!Send(new AcceleratedVideoDecoderHostMsg_DismissPictureBuffer(
          host_route_id_, picture_buffer_id))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer) "
                << "failed";
  }
}

void GpuVideoDecodeAccelerator::PictureReady(
    const media::Picture& picture) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_PictureReady(
          host_route_id_,
          picture.picture_buffer_id(),
          picture.bitstream_buffer_id()))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_PictureReady) failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  if (init_done_msg_) {
    // If we get an error while we're initializing, NotifyInitializeDone won't
    // be called, so we need to send the reply (with an error) here.
    init_done_msg_->set_reply_error();
    if (!Send(init_done_msg_))
      DLOG(ERROR) << "Send(init_done_msg_) failed";
    init_done_msg_ = NULL;
  }
  if (!Send(new AcceleratedVideoDecoderHostMsg_ErrorNotification(
          host_route_id_, error))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ErrorNotification) "
                << "failed";
  }
}

void GpuVideoDecodeAccelerator::Initialize(
    const media::VideoCodecProfile profile,
    IPC::Message* init_done_msg) {
  DCHECK(!video_decode_accelerator_.get());
  DCHECK(!init_done_msg_);
  DCHECK(init_done_msg);
  init_done_msg_ = init_done_msg;

#if (defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)) || defined(OS_WIN)
  DCHECK(stub_ && stub_->decoder());
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_WIN7) {
    NOTIMPLEMENTED() << "HW video decode acceleration not available.";
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  DLOG(INFO) << "Initializing DXVA HW decoder for windows.";
  DXVAVideoDecodeAccelerator* video_decoder =
      new DXVAVideoDecodeAccelerator(this);
#else  // OS_WIN
  OmxVideoDecodeAccelerator* video_decoder =
      new OmxVideoDecodeAccelerator(this);
  video_decoder->SetEglState(
      gfx::GLSurfaceEGL::GetHardwareDisplay(),
      stub_->decoder()->GetGLContext()->GetHandle());
#endif  // OS_WIN
  video_decode_accelerator_ = video_decoder;
  if (!video_decode_accelerator_->Initialize(profile))
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
#else  // Update RenderViewImpl::createMediaPlayer when adding clauses.
  NOTIMPLEMENTED() << "HW video decode acceleration not available.";
  NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
#endif  // defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
}

void GpuVideoDecodeAccelerator::OnDecode(
    base::SharedMemoryHandle handle, int32 id, int32 size) {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Decode(media::BitstreamBuffer(id, handle, size));
}

void GpuVideoDecodeAccelerator::OnAssignPictureBuffers(
      const std::vector<int32>& buffer_ids,
      const std::vector<uint32>& texture_ids,
      const std::vector<gfx::Size>& sizes) {
  DCHECK(stub_ && stub_->decoder());  // Ensure already Initialize()'d.
  gpu::gles2::GLES2Decoder* command_decoder = stub_->decoder();
  gpu::gles2::TextureManager* texture_manager =
      command_decoder->GetContextGroup()->texture_manager();

  std::vector<media::PictureBuffer> buffers;
  for (uint32 i = 0; i < buffer_ids.size(); ++i) {
    gpu::gles2::TextureManager::TextureInfo* info =
        texture_manager->GetTextureInfo(texture_ids[i]);
    if (!info) {
      DLOG(FATAL) << "Failed to find texture id " << texture_ids[i];
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    if (!texture_manager->ClearRenderableLevels(command_decoder, info)) {
      DLOG(FATAL) << "Failed to Clear texture id " << texture_ids[i];
      NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
      return;
    }
    uint32 service_texture_id;
    if (!command_decoder->GetServiceTextureId(
            texture_ids[i], &service_texture_id)) {
      DLOG(FATAL) << "Failed to translate texture!";
      NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
      return;
    }
    buffers.push_back(media::PictureBuffer(
        buffer_ids[i], sizes[i], service_texture_id));
  }
  video_decode_accelerator_->AssignPictureBuffers(buffers);
}

void GpuVideoDecodeAccelerator::OnReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->ReusePictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::OnFlush() {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Flush();
}

void GpuVideoDecodeAccelerator::OnReset() {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Reset();
}

void GpuVideoDecodeAccelerator::OnDestroy() {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Destroy();
}

void GpuVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed(
          host_route_id_, bitstream_buffer_id))) {
    DLOG(ERROR)
        << "Send(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed) "
        << "failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyInitializeDone() {
  if (!Send(init_done_msg_))
    DLOG(ERROR) << "Send(init_done_msg_) failed";
  init_done_msg_ = NULL;
}

void GpuVideoDecodeAccelerator::NotifyFlushDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_FlushDone(host_route_id_)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_FlushDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyResetDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ResetDone(host_route_id_)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ResetDone) failed";
}

bool GpuVideoDecodeAccelerator::Send(IPC::Message* message) {
  DCHECK(sender_);
  return sender_->Send(message);
}
