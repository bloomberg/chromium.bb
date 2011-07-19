// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gpu_video_decode_accelerator.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
#include "content/common/gpu/media/omx_video_decode_accelerator.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#endif
#include "ui/gfx/size.h"

GpuVideoDecodeAccelerator::GpuVideoDecodeAccelerator(
    IPC::Message::Sender* sender,
    int32 host_route_id,
    GpuCommandBufferStub* stub)
    : sender_(sender),
      host_route_id_(host_route_id),
      stub_(stub),
      video_decode_accelerator_(NULL) {
  // stub_ owns and will always outlive this object.
  stub_->AddSetTokenCallback(base::Bind(
      &GpuVideoDecodeAccelerator::OnSetToken, base::Unretained(this)));
}

GpuVideoDecodeAccelerator::~GpuVideoDecodeAccelerator() {
  STLDeleteElements(&deferred_messages_);
  // TODO(fischman/vrk): We need to synchronously wait for the OMX decoder
  // to finish shutting down.
}

void GpuVideoDecodeAccelerator::OnSetToken(int32 token) {
  // Note: this always retries all deferred messages on every token arrival.
  // There's an optimization to be done here by only trying messages which are
  // waiting for tokens which are earlier than |token|.
  std::vector<IPC::Message*> deferred_messages_copy;
  std::swap(deferred_messages_copy, deferred_messages_);
  for (size_t i = 0; i < deferred_messages_copy.size(); ++i)
    OnMessageReceived(*deferred_messages_copy[i]);
  STLDeleteElements(&deferred_messages_copy);
}

bool GpuVideoDecodeAccelerator::DeferMessageIfNeeded(
    const IPC::Message& msg, bool* deferred) {
  // Only consider deferring for message types that need it.
  switch (msg.type()) {
    case AcceleratedVideoDecoderMsg_Decode::ID:
    case AcceleratedVideoDecoderMsg_AssignPictureBuffers::ID:
    case AcceleratedVideoDecoderMsg_ReusePictureBuffer::ID:
    case AcceleratedVideoDecoderMsg_Flush::ID:
    case AcceleratedVideoDecoderMsg_Reset::ID:
    case AcceleratedVideoDecoderMsg_Destroy::ID:
      break;
    default:
      return false;
  }

  gpu::ReadWriteTokens tokens;
  void* iter = NULL;
  if (!IPC::ParamTraits<gpu::ReadWriteTokens>::Read(&msg, &iter, &tokens))
    return false;
  if (tokens.InRange(stub_->token())) {
    deferred_messages_.push_back(new IPC::Message(msg));
    *deferred = true;
  } else {
    *deferred = false;
  }
  return true;
}

bool GpuVideoDecodeAccelerator::OnMessageReceived(const IPC::Message& msg) {
  bool deferred = false;
  if (!DeferMessageIfNeeded(msg, &deferred))
    return false;
  if (deferred)
    return true;

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

void GpuVideoDecodeAccelerator::OnChannelConnected(int32 peer_pid) {
  // TODO(vmr): Do we have to react on channel connections?
}

void GpuVideoDecodeAccelerator::OnChannelError() {
  // TODO(vmr): Do we have to react on channel errors?
}

void GpuVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32 requested_num_of_buffers, const gfx::Size& dimensions) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers(
          host_route_id_, requested_num_of_buffers, dimensions))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers) "
               << "failed";
  }
}

void GpuVideoDecodeAccelerator::DismissPictureBuffer(
    int32 picture_buffer_id) {
  // Notify client that picture buffer is now unused.
  if (!Send(new AcceleratedVideoDecoderHostMsg_DismissPictureBuffer(
          host_route_id_, picture_buffer_id))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer) "
               << "failed";
  }
}

void GpuVideoDecodeAccelerator::PictureReady(
    const media::Picture& picture) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_PictureReady(
          host_route_id_,
          picture.picture_buffer_id(),
          picture.bitstream_buffer_id()))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_PictureReady) failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyEndOfStream() {
  Send(new AcceleratedVideoDecoderHostMsg_EndOfStream(host_route_id_));
}

void GpuVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ErrorNotification(
          host_route_id_, error))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ErrorNotification) "
               << "failed";
  }
}

void GpuVideoDecodeAccelerator::Initialize(const std::vector<uint32>& configs) {
  DCHECK(!video_decode_accelerator_.get());
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
  DCHECK(stub_ && stub_->scheduler());
  OmxVideoDecodeAccelerator* omx_decoder = new OmxVideoDecodeAccelerator(this);
  omx_decoder->SetEglState(
      gfx::GLSurfaceEGL::GetDisplay(),
      stub_->scheduler()->decoder()->GetGLContext()->GetHandle());
  video_decode_accelerator_ = omx_decoder;
  video_decode_accelerator_->Initialize(configs);
#else
  NOTIMPLEMENTED() << "HW video decode acceleration not available.";
#endif  // defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
}

void GpuVideoDecodeAccelerator::OnDecode(
    const gpu::ReadWriteTokens&, /* tokens */
    base::SharedMemoryHandle handle, int32 id, int32 size) {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Decode(media::BitstreamBuffer(id, handle, size));
}

void GpuVideoDecodeAccelerator::OnAssignPictureBuffers(
      const gpu::ReadWriteTokens& /* tokens */,
      const std::vector<int32>& buffer_ids,
      const std::vector<uint32>& texture_ids,
      const std::vector<gfx::Size>& sizes) {
  DCHECK(stub_ && stub_->scheduler());  // Ensure already Initialize()'d.
  gpu::gles2::GLES2Decoder* command_decoder = stub_->scheduler()->decoder();

  std::vector<media::PictureBuffer> buffers;
  for (uint32 i = 0; i < buffer_ids.size(); ++i) {
    uint32 service_texture_id;
    if (!command_decoder->GetServiceTextureId(
            texture_ids[i], &service_texture_id)) {
      // TODO(vrk): Send an error for invalid GLES buffers.
      LOG(DFATAL) << "Failed to translate texture!";
      return;
    }
    buffers.push_back(media::PictureBuffer(
        buffer_ids[i], sizes[i], service_texture_id));
  }
  video_decode_accelerator_->AssignPictureBuffers(buffers);
}

void GpuVideoDecodeAccelerator::OnReusePictureBuffer(
    const gpu::ReadWriteTokens& /* tokens */,
    int32 picture_buffer_id) {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->ReusePictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::OnFlush(
    const gpu::ReadWriteTokens& /* tokens */) {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Flush();
}

void GpuVideoDecodeAccelerator::OnReset(
    const gpu::ReadWriteTokens& /* tokens */) {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Reset();
}

void GpuVideoDecodeAccelerator::OnDestroy(
    const gpu::ReadWriteTokens& /* tokens */) {
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
  if (!Send(new AcceleratedVideoDecoderHostMsg_InitializeDone(host_route_id_)))
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_InitializeDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyFlushDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_FlushDone(host_route_id_)))
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_FlushDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyResetDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ResetDone(host_route_id_)))
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ResetDone) failed";
}

bool GpuVideoDecodeAccelerator::Send(IPC::Message* message) {
  DCHECK(sender_);
  return sender_->Send(message);
}
