// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu_video_decode_accelerator_host.h"

#include "base/logging.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

using media::VideoDecodeAccelerator;
using media::VideoDecodeAcceleratorCallback;

GpuVideoDecodeAcceleratorHost::GpuVideoDecodeAcceleratorHost(
    MessageRouter* router,
    IPC::Message::Sender* ipc_sender,
    int32 decoder_host_id,
    VideoDecodeAccelerator::Client* client)
    : router_(router),
      ipc_sender_(ipc_sender),
      decoder_host_id_(decoder_host_id),
      decoder_id_(0),
      client_(client) {
}

GpuVideoDecodeAcceleratorHost::~GpuVideoDecodeAcceleratorHost() {}

void GpuVideoDecodeAcceleratorHost::OnChannelConnected(int32 peer_pid) {
}

void GpuVideoDecodeAcceleratorHost::OnChannelError() {
  ipc_sender_ = NULL;
}

bool GpuVideoDecodeAcceleratorHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAcceleratorHost, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed,
                        OnBitstreamBufferProcessed)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers,
                        OnProvidePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_CreateDone,
                        OnCreateDone)
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

void GpuVideoDecodeAcceleratorHost::GetConfigs(
    const std::vector<uint32>& requested_configs,
    std::vector<uint32>* matched_configs) {
  // TODO(vrk): Need to rethink GetConfigs.
  NOTIMPLEMENTED();
}

bool GpuVideoDecodeAcceleratorHost::Initialize(
    const std::vector<uint32>& configs) {
  router_->AddRoute(decoder_host_id_, this);

  // Temporarily save configs for after create is done and we're
  // ready to initialize.
  configs_ = configs;

  if (!ipc_sender_->Send(new GpuChannelMsg_CreateVideoDecoder(
          decoder_id_, configs))) {
    LOG(ERROR) << "Send(GpuChannelMsg_CreateVideoDecoder) failed";
    return false;
  }
  return true;
}

bool GpuVideoDecodeAcceleratorHost::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
  return false;
}

void GpuVideoDecodeAcceleratorHost::AssignGLESBuffers(
    const std::vector<media::GLESBuffer>& buffers) {
  // Rearrange data for IPC command.
  std::vector<int32> buffer_ids;
  std::vector<uint32> texture_ids;
  std::vector<uint32> context_ids;
  std::vector<gfx::Size> sizes;
  for (uint32 i = 0; i < buffers.size(); i++) {
    const media::BufferInfo& info = buffers[i].buffer_info();
    texture_ids.push_back(buffers[i].texture_id());
    context_ids.push_back(buffers[i].context_id());
    buffer_ids.push_back(info.id());
    sizes.push_back(info.size());
  }
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_AssignGLESBuffers(
          decoder_id_, buffer_ids, texture_ids, context_ids, sizes))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_AssignGLESBuffers) failed";
  }
}

void GpuVideoDecodeAcceleratorHost::AssignSysmemBuffers(
    const std::vector<media::SysmemBuffer>& buffers) {
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
}

void GpuVideoDecodeAcceleratorHost::ReusePictureBuffer(
    int32 picture_buffer_id) {
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_ReusePictureBuffer(
          decoder_id_, picture_buffer_id))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_ReusePictureBuffer) failed";
  }
}

bool GpuVideoDecodeAcceleratorHost::Flush() {
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_Flush(decoder_id_))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_Flush) failed";
    return false;
  }
  return true;
}

bool GpuVideoDecodeAcceleratorHost::Abort() {
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_Abort(decoder_id_))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_Abort) failed";
    return false;
  }
  return true;
}

void GpuVideoDecodeAcceleratorHost::OnBitstreamBufferProcessed(
    int32 bitstream_buffer_id) {
  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnProvidePictureBuffer(
    uint32 num_requested_buffers,
    const gfx::Size& buffer_size,
    int32 mem_type) {
  media::VideoDecodeAccelerator::MemoryType converted_mem_type =
      static_cast<media::VideoDecodeAccelerator::MemoryType>(mem_type);
  client_->ProvidePictureBuffers(
      num_requested_buffers, buffer_size, converted_mem_type);
}

void GpuVideoDecodeAcceleratorHost::OnDismissPictureBuffer(
    int32 picture_buffer_id) {
  client_->DismissPictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnCreateDone(int32 decoder_id) {
  decoder_id_ = decoder_id;
  if (!ipc_sender_->Send(new AcceleratedVideoDecoderMsg_Initialize(
      decoder_id_, configs_))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderMsg_Initialize) failed";
  }
}

void GpuVideoDecodeAcceleratorHost::OnPictureReady(
    int32 picture_buffer_id, int32 bitstream_buffer_id,
    const gfx::Size& visible_size, const gfx::Size& decoded_size) {
  media::Picture picture(
      picture_buffer_id, bitstream_buffer_id, visible_size, decoded_size);
  client_->PictureReady(picture);
}

void GpuVideoDecodeAcceleratorHost::OnFlushDone() {
  client_->NotifyFlushDone();
}

void GpuVideoDecodeAcceleratorHost::OnAbortDone() {
  client_->NotifyAbortDone();
}

void GpuVideoDecodeAcceleratorHost::OnEndOfStream() {
  client_->NotifyEndOfStream();
}

void GpuVideoDecodeAcceleratorHost::OnErrorNotification(uint32 error) {
  client_->NotifyError(
      static_cast<media::VideoDecodeAccelerator::Error>(error));
}
