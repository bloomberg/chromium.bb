// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_video_decode_accelerator_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"

using media::VideoDecodeAccelerator;

GpuVideoDecodeAcceleratorHost::GpuVideoDecodeAcceleratorHost(
    IPC::Message::Sender* ipc_sender,
    int32 command_buffer_route_id,
    gpu::CommandBufferHelper* cmd_buffer_helper,
    VideoDecodeAccelerator::Client* client)
    : ipc_sender_(ipc_sender),
      command_buffer_route_id_(command_buffer_route_id),
      cmd_buffer_helper_(cmd_buffer_helper),
      client_(client) {
  DCHECK(ipc_sender_);
  DCHECK(cmd_buffer_helper_);
  DCHECK(client_);
  DCHECK(RenderThread::current());
  DCHECK_EQ(RenderThread::current()->message_loop(), MessageLoop::current());
}

GpuVideoDecodeAcceleratorHost::~GpuVideoDecodeAcceleratorHost() {}

void GpuVideoDecodeAcceleratorHost::OnChannelError() {
  cmd_buffer_helper_ = NULL;
  ipc_sender_ = NULL;
}

bool GpuVideoDecodeAcceleratorHost::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAcceleratorHost, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed,
                        OnBitstreamBufferProcessed)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers,
                        OnProvidePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_InitializeDone,
                        OnInitializeDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_PictureReady,
                        OnPictureReady)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_FlushDone,
                        OnFlushDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ResetDone,
                        OnResetDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_EndOfStream,
                        OnEndOfStream)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ErrorNotification,
                        OnErrorNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

gpu::ReadWriteTokens GpuVideoDecodeAcceleratorHost::SyncTokens() {
  DCHECK(CalledOnValidThread());
  DCHECK(cmd_buffer_helper_);

  // SyncTokens() is only called when creating new IPC messages.
  // Calling SyncTokens() with a NULL command buffer means that the client is
  // trying to send an IPC message over a channel that is no longer
  // valid, so the error message that is sent in Send() for a null
  // |ipc_sender| also accounts for the error case below.
  if (!cmd_buffer_helper_)
    return gpu::ReadWriteTokens();

  // Note that the order below matters.  InsertToken() must happen before
  // Flush() and last_token_read() should be read before InsertToken().
  int32 read = cmd_buffer_helper_->last_token_read();
  int32 written = cmd_buffer_helper_->InsertToken();
  cmd_buffer_helper_->Flush();
  return gpu::ReadWriteTokens(read, written);
}

bool GpuVideoDecodeAcceleratorHost::Initialize(
    const std::vector<uint32>& configs) {
  NOTREACHED();
  return true;
}

void GpuVideoDecodeAcceleratorHost::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedVideoDecoderMsg_Decode(
      command_buffer_route_id_, SyncTokens(), bitstream_buffer.handle(),
      bitstream_buffer.id(), bitstream_buffer.size()));
}

void GpuVideoDecodeAcceleratorHost::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(CalledOnValidThread());
  // Rearrange data for IPC command.
  std::vector<int32> buffer_ids;
  std::vector<uint32> texture_ids;
  std::vector<gfx::Size> sizes;
  for (uint32 i = 0; i < buffers.size(); i++) {
    const media::PictureBuffer& buffer = buffers[i];
    texture_ids.push_back(buffer.texture_id());
    buffer_ids.push_back(buffer.id());
    sizes.push_back(buffer.size());
  }
  Send(new AcceleratedVideoDecoderMsg_AssignPictureBuffers(
      command_buffer_route_id_, SyncTokens(), buffer_ids, texture_ids, sizes));
}

void GpuVideoDecodeAcceleratorHost::ReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedVideoDecoderMsg_ReusePictureBuffer(
      command_buffer_route_id_, SyncTokens(), picture_buffer_id));
}

void GpuVideoDecodeAcceleratorHost::Flush() {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedVideoDecoderMsg_Flush(
      command_buffer_route_id_, SyncTokens()));
}

void GpuVideoDecodeAcceleratorHost::Reset() {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedVideoDecoderMsg_Reset(
      command_buffer_route_id_, SyncTokens()));
}

void GpuVideoDecodeAcceleratorHost::Destroy() {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedVideoDecoderMsg_Destroy(
      command_buffer_route_id_, SyncTokens()));
}

void GpuVideoDecodeAcceleratorHost::Send(IPC::Message* message) {
  // After OnChannelError is called, the client should no longer send
  // messages to the gpu channel through this object.
  DCHECK(ipc_sender_);
  if (!ipc_sender_ || !ipc_sender_->Send(message)) {
    DLOG(ERROR) << "Send(" << message->type() << ") failed";
    OnErrorNotification(PLATFORM_FAILURE);
  }
}

void GpuVideoDecodeAcceleratorHost::OnBitstreamBufferProcessed(
    int32 bitstream_buffer_id) {
  DCHECK(CalledOnValidThread());
  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnProvidePictureBuffer(
    uint32 num_requested_buffers,
    const gfx::Size& buffer_size) {
  DCHECK(CalledOnValidThread());
  client_->ProvidePictureBuffers(num_requested_buffers, buffer_size);
}

void GpuVideoDecodeAcceleratorHost::OnDismissPictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  client_->DismissPictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnInitializeDone() {
  DCHECK(CalledOnValidThread());
  client_->NotifyInitializeDone();
}

void GpuVideoDecodeAcceleratorHost::OnPictureReady(
    int32 picture_buffer_id, int32 bitstream_buffer_id) {
  DCHECK(CalledOnValidThread());
  media::Picture picture(picture_buffer_id, bitstream_buffer_id);
  client_->PictureReady(picture);
}

void GpuVideoDecodeAcceleratorHost::OnFlushDone() {
  DCHECK(CalledOnValidThread());
  client_->NotifyFlushDone();
}

void GpuVideoDecodeAcceleratorHost::OnResetDone() {
  DCHECK(CalledOnValidThread());
  client_->NotifyResetDone();
}

void GpuVideoDecodeAcceleratorHost::OnEndOfStream() {
  DCHECK(CalledOnValidThread());
  client_->NotifyEndOfStream();
}

void GpuVideoDecodeAcceleratorHost::OnErrorNotification(uint32 error) {
  DCHECK(CalledOnValidThread());
  client_->NotifyError(
      static_cast<media::VideoDecodeAccelerator::Error>(error));
}
