// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif  // OS_WIN

using media::VideoDecodeAccelerator;
namespace content {

GpuVideoDecodeAcceleratorHost::GpuVideoDecodeAcceleratorHost(
    GpuChannelHost* channel,
    int32 decoder_route_id,
    VideoDecodeAccelerator::Client* client)
    : channel_(channel),
      decoder_route_id_(decoder_route_id),
      client_(client) {
  DCHECK(channel_);
  DCHECK(client_);
}

void GpuVideoDecodeAcceleratorHost::OnChannelError() {
  DLOG(ERROR) << "GpuVideoDecodeAcceleratorHost::OnChannelError()";
  OnErrorNotification(PLATFORM_FAILURE);
  channel_ = NULL;
}

bool GpuVideoDecodeAcceleratorHost::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAcceleratorHost, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed,
                        OnBitstreamBufferProcessed)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers,
                        OnProvidePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_PictureReady,
                        OnPictureReady)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_FlushDone,
                        OnFlushDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ResetDone,
                        OnResetDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ErrorNotification,
                        OnErrorNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

bool GpuVideoDecodeAcceleratorHost::Initialize(
    media::VideoCodecProfile profile) {
  NOTREACHED();
  return true;
}

void GpuVideoDecodeAcceleratorHost::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(CalledOnValidThread());
  // Can happen if a decode task was posted before an error was delivered.
  if (!channel_)
    return;
  base::SharedMemoryHandle buffer_handle = bitstream_buffer.handle();
#if defined(OS_WIN)
  if (!BrokerDuplicateHandle(bitstream_buffer.handle(),
                             channel_->gpu_pid(),
                             &buffer_handle, 0,
                             DUPLICATE_SAME_ACCESS)) {
    NOTREACHED() << "Failed to duplicate buffer handler";
    return;
  }
#endif  // OS_WIN

  Send(new AcceleratedVideoDecoderMsg_Decode(
      decoder_route_id_, buffer_handle, bitstream_buffer.id(),
      bitstream_buffer.size()));
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
      decoder_route_id_, buffer_ids, texture_ids, sizes));
}

void GpuVideoDecodeAcceleratorHost::ReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedVideoDecoderMsg_ReusePictureBuffer(
      decoder_route_id_, picture_buffer_id));
}

void GpuVideoDecodeAcceleratorHost::Flush() {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedVideoDecoderMsg_Flush(decoder_route_id_));
}

void GpuVideoDecodeAcceleratorHost::Reset() {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedVideoDecoderMsg_Reset(decoder_route_id_));
}

void GpuVideoDecodeAcceleratorHost::Destroy() {
  DCHECK(CalledOnValidThread());
  if (channel_)
    channel_->RemoveRoute(decoder_route_id_);
  client_ = NULL;
  Send(new AcceleratedVideoDecoderMsg_Destroy(decoder_route_id_));
  delete this;
}

GpuVideoDecodeAcceleratorHost::~GpuVideoDecodeAcceleratorHost() {
  DCHECK(CalledOnValidThread());
  DCHECK(!client_) << "destructor called without Destroy being called!";
}

void GpuVideoDecodeAcceleratorHost::Send(IPC::Message* message) {
  // After OnChannelError is called, the client should no longer send
  // messages to the gpu channel through this object.  But queued posted tasks
  // can still be draining, so we're forgiving and simply ignore them.
  bool error = false;
  uint32 message_type = message->type();
  if (!channel_) {
    delete message;
    DLOG(ERROR) << "Send(" << message_type << ") after error ignored";
    error = true;
  } else if (!channel_->Send(message)) {
    DLOG(ERROR) << "Send(" << message_type << ") failed";
    error = true;
  }
  if (error)
    OnErrorNotification(PLATFORM_FAILURE);
}

void GpuVideoDecodeAcceleratorHost::OnBitstreamBufferProcessed(
    int32 bitstream_buffer_id) {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnProvidePictureBuffer(
    uint32 num_requested_buffers,
    const gfx::Size& buffer_size,
    uint32 texture_target) {
  DCHECK(CalledOnValidThread());
  if (client_) {
    client_->ProvidePictureBuffers(
        num_requested_buffers, buffer_size, texture_target);
  }
}

void GpuVideoDecodeAcceleratorHost::OnDismissPictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->DismissPictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnPictureReady(
    int32 picture_buffer_id, int32 bitstream_buffer_id) {
  DCHECK(CalledOnValidThread());
  if (!client_)
    return;
  media::Picture picture(picture_buffer_id, bitstream_buffer_id);
  client_->PictureReady(picture);
}

void GpuVideoDecodeAcceleratorHost::OnFlushDone() {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->NotifyFlushDone();
}

void GpuVideoDecodeAcceleratorHost::OnResetDone() {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->NotifyResetDone();
}

void GpuVideoDecodeAcceleratorHost::OnErrorNotification(uint32 error) {
  DCHECK(CalledOnValidThread());
  if (!client_)
    return;
  client_->NotifyError(
      static_cast<media::VideoDecodeAccelerator::Error>(error));
  client_ = NULL;
}

}  // namespace content
