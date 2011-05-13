// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_video_decode_accelerator.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/gfx/size.h"

GpuVideoDecodeAccelerator::GpuVideoDecodeAccelerator(
    IPC::Message::Sender* sender,
    int32 host_route_id)
    : sender_(sender),
      route_id_(host_route_id),
      video_decode_accelerator_(NULL) {
}

GpuVideoDecodeAccelerator::~GpuVideoDecodeAccelerator() {}

bool GpuVideoDecodeAccelerator::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAccelerator, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_GetConfigs, OnGetConfigs)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Decode, OnDecode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_AssignSysmemBuffers,
                        OnAssignSysmemBuffers)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_AssignGLESBuffers,
                        OnAssignGLESBuffers)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                        OnReusePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Flush, OnFlush)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Abort, OnAbort)
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
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    media::VideoDecodeAccelerator::MemoryType type) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers(
          route_id_, requested_num_of_buffers, dimensions, type))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers) "
               << "failed";
  }
}

void GpuVideoDecodeAccelerator::DismissPictureBuffer(
    int32 picture_buffer_id) {
  // TODO(vrk): Unmap picture buffer.
  NOTIMPLEMENTED();

  // Notify client that picture buffer is now unused.
  if (!Send(new AcceleratedVideoDecoderHostMsg_DismissPictureBuffer(
          route_id_, picture_buffer_id))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer) "
               << "failed";
  }
}

void GpuVideoDecodeAccelerator::PictureReady(
    const media::Picture& picture) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_PictureReady(
          route_id_,
          picture.picture_buffer_id(),
          picture.bitstream_buffer_id(),
          picture.visible_size(),
          picture.decoded_size()))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_PictureReady) failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyEndOfStream() {
  Send(new AcceleratedVideoDecoderHostMsg_EndOfStream(route_id_));
}

void GpuVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ErrorNotification(
          route_id_, error))) {
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ErrorNotification) "
               << "failed";
  }
}

void GpuVideoDecodeAccelerator::OnGetConfigs(
    const std::vector<uint32>& requested, std::vector<uint32>* matched) {
  // TODO(vrk): Need to rethink GetConfigs.
  NOTIMPLEMENTED();
}

void GpuVideoDecodeAccelerator::OnInitialize(
    const std::vector<uint32>& configs) {
  if (!video_decode_accelerator_)
    return;

  video_decode_accelerator_->Initialize(configs);
}

void GpuVideoDecodeAccelerator::OnDecode(int32 id,
                                         base::SharedMemoryHandle handle,
                                         int32 size) {
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
}

void GpuVideoDecodeAccelerator::OnAssignGLESBuffers(
    const std::vector<int32> buffer_ids,
    const std::vector<uint32> texture_ids,
    const std::vector<uint32> context_ids,
    const std::vector<gfx::Size> sizes) {
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
}

void GpuVideoDecodeAccelerator::OnAssignSysmemBuffers(
    const std::vector<int32> buffer_ids,
    const std::vector<base::SharedMemoryHandle> data,
    const std::vector<gfx::Size> sizes) {
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
}

void GpuVideoDecodeAccelerator::OnReusePictureBuffer(int32 picture_buffer_id) {
  if (!video_decode_accelerator_)
    return;

  video_decode_accelerator_->ReusePictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::OnFlush() {
  if (!video_decode_accelerator_)
    return;

  if (!video_decode_accelerator_->Flush()) {
    NotifyError(
        media::VideoDecodeAccelerator::VIDEODECODERERROR_UNEXPECTED_FLUSH);
  }
}

void GpuVideoDecodeAccelerator::OnAbort() {
  if (!video_decode_accelerator_)
    return;

  video_decode_accelerator_->Abort();
}

void GpuVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
}

void GpuVideoDecodeAccelerator::NotifyFlushDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_FlushDone(route_id_)))
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_FlushDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyAbortDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_AbortDone(route_id_)))
    LOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_AbortDone) failed";
}

bool GpuVideoDecodeAccelerator::Send(IPC::Message* message) {
  DCHECK(sender_);
  return sender_->Send(message);
}
