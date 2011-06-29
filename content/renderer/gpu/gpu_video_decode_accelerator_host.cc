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
    MessageRouter* router,
    IPC::Message::Sender* ipc_sender,
    int32 decoder_host_id,
    int32 command_buffer_route_id,
    gpu::CommandBufferHelper* cmd_buffer_helper,
    VideoDecodeAccelerator::Client* client)
    : router_(router),
      ipc_sender_(ipc_sender),
      decoder_host_id_(decoder_host_id),
      decoder_id_(-1),
      command_buffer_route_id_(command_buffer_route_id),
      cmd_buffer_helper_(cmd_buffer_helper),
      client_(client) {
  DCHECK(RenderThread::current());
  DCHECK_EQ(RenderThread::current()->message_loop(), MessageLoop::current());
}

GpuVideoDecodeAcceleratorHost::~GpuVideoDecodeAcceleratorHost() {}

void GpuVideoDecodeAcceleratorHost::OnChannelConnected(int32 peer_pid) {
}

void GpuVideoDecodeAcceleratorHost::OnChannelError() {
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
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_CreateDone,
                        OnCreateDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_InitializeDone,
                        OnInitializeDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_PictureReady,
                        OnPictureReady)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_FlushDone,
                        OnFlushDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_AbortDone,
                        OnAbortDone)
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
  // Note that the order below matters.  InsertToken() must happen before
  // Flush() and last_token_read() should be read before InsertToken().
  int32 read = cmd_buffer_helper_->last_token_read();
  int32 written = cmd_buffer_helper_->InsertToken();
  cmd_buffer_helper_->Flush();
  return gpu::ReadWriteTokens(read, written);
}

bool GpuVideoDecodeAcceleratorHost::GetConfigs(
    const std::vector<uint32>& requested_configs,
    std::vector<uint32>* matched_configs) {
  // TODO(vrk): Need to rethink GetConfigs.
  NOTIMPLEMENTED();
  return true;
}

bool GpuVideoDecodeAcceleratorHost::Initialize(
    const std::vector<uint32>& configs) {
  DCHECK(CalledOnValidThread());
  router_->AddRoute(decoder_host_id_, this);

  // Temporarily save configs for after create is done and we're
  // ready to initialize.
  configs_ = configs;
  if (!ipc_sender_->Send(new GpuChannelMsg_CreateVideoDecoder(
          decoder_host_id_, command_buffer_route_id_, configs))) {
    LOG(ERROR) << "Send(GpuChannelMsg_CreateVideoDecoder) failed";
    return false;
  }
  return true;
}

void GpuVideoDecodeAcceleratorHost::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(CalledOnValidThread());
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_Decode(
          decoder_id_, SyncTokens(), bitstream_buffer.handle(),
          bitstream_buffer.id(), bitstream_buffer.size()))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_Decode) failed";
    // TODO(fischman/vrk): signal error to client.
    return;
  }
}

void GpuVideoDecodeAcceleratorHost::AssignGLESBuffers(
    const std::vector<media::GLESBuffer>& buffers) {
  DCHECK(CalledOnValidThread());
  // Rearrange data for IPC command.
  std::vector<int32> buffer_ids;
  std::vector<uint32> texture_ids;
  std::vector<gfx::Size> sizes;
  for (uint32 i = 0; i < buffers.size(); i++) {
    const media::GLESBuffer& buffer = buffers[i];
    texture_ids.push_back(buffer.texture_id());
    buffer_ids.push_back(buffer.id());
    sizes.push_back(buffer.size());
  }
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_AssignTextures(
          decoder_id_, SyncTokens(), buffer_ids, texture_ids, sizes))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_AssignGLESBuffers) failed";
  }
}

void GpuVideoDecodeAcceleratorHost::AssignSysmemBuffers(
    const std::vector<media::SysmemBuffer>& buffers) {
  DCHECK(CalledOnValidThread());
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
}

void GpuVideoDecodeAcceleratorHost::ReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_ReusePictureBuffer(
          decoder_id_, SyncTokens(), picture_buffer_id))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_ReusePictureBuffer) failed";
  }
}

void GpuVideoDecodeAcceleratorHost::Flush() {
  DCHECK(CalledOnValidThread());
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_Flush(
          decoder_id_, SyncTokens()))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_Flush) failed";
    // TODO(fischman/vrk): signal error to client.
    return;
  }
}

void GpuVideoDecodeAcceleratorHost::Abort() {
  DCHECK(CalledOnValidThread());
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_Abort(
          decoder_id_, SyncTokens()))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_Abort) failed";
    // TODO(fischman/vrk): signal error to client.
    return;
  }
}

void GpuVideoDecodeAcceleratorHost::OnBitstreamBufferProcessed(
    int32 bitstream_buffer_id) {
  DCHECK(CalledOnValidThread());
  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnProvidePictureBuffer(
    uint32 num_requested_buffers,
    const gfx::Size& buffer_size,
    int32 mem_type) {
  DCHECK(CalledOnValidThread());
  media::VideoDecodeAccelerator::MemoryType converted_mem_type =
      static_cast<media::VideoDecodeAccelerator::MemoryType>(mem_type);
  client_->ProvidePictureBuffers(
      num_requested_buffers, buffer_size, converted_mem_type);
}

void GpuVideoDecodeAcceleratorHost::OnDismissPictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  client_->DismissPictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnCreateDone(int32 decoder_id) {
  DCHECK(CalledOnValidThread());
  decoder_id_ = decoder_id;
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_Initialize(
          decoder_id_, SyncTokens(), configs_))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_Initialize) failed";
  }
}

void GpuVideoDecodeAcceleratorHost::OnInitializeDone() {
  DCHECK(CalledOnValidThread());
  client_->NotifyInitializeDone();
}

void GpuVideoDecodeAcceleratorHost::OnPictureReady(
    int32 picture_buffer_id, int32 bitstream_buffer_id,
    const gfx::Size& visible_size, const gfx::Size& decoded_size) {
  DCHECK(CalledOnValidThread());
  media::Picture picture(
      picture_buffer_id, bitstream_buffer_id, visible_size, decoded_size);
  client_->PictureReady(picture);
}

void GpuVideoDecodeAcceleratorHost::OnFlushDone() {
  DCHECK(CalledOnValidThread());
  client_->NotifyFlushDone();
}

void GpuVideoDecodeAcceleratorHost::OnAbortDone() {
  DCHECK(CalledOnValidThread());
  client_->NotifyAbortDone();
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
