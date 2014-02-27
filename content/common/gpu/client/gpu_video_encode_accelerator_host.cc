// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/media/gpu_video_encode_accelerator.h"
#include "media/base/video_frame.h"

namespace content {

GpuVideoEncodeAcceleratorHost::GpuVideoEncodeAcceleratorHost(
    const scoped_refptr<GpuChannelHost>& gpu_channel_host,
    int32 route_id)
    : client_(NULL),
      channel_(gpu_channel_host),
      route_id_(route_id),
      next_frame_id_(0) {
  channel_->AddRoute(route_id_, AsWeakPtr());
}

GpuVideoEncodeAcceleratorHost::~GpuVideoEncodeAcceleratorHost() {
  if (channel_)
    channel_->RemoveRoute(route_id_);
}

// static
std::vector<media::VideoEncodeAccelerator::SupportedProfile>
GpuVideoEncodeAcceleratorHost::GetSupportedProfiles() {
  return GpuVideoEncodeAccelerator::GetSupportedProfiles();
}

bool GpuVideoEncodeAcceleratorHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoEncodeAcceleratorHost, message)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_NotifyInitializeDone,
                        OnNotifyInitializeDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_RequireBitstreamBuffers,
                        OnRequireBitstreamBuffers)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_NotifyInputDone,
                        OnNotifyInputDone)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_BitstreamBufferReady,
                        OnBitstreamBufferReady)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderHostMsg_NotifyError,
                        OnNotifyError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  // See OnNotifyError for why |this| mustn't be used after OnNotifyError might
  // have been called above.
  return handled;
}

void GpuVideoEncodeAcceleratorHost::OnChannelError() {
  DLOG(ERROR) << "OnChannelError()";
  if (channel_) {
    channel_->RemoveRoute(route_id_);
    channel_ = NULL;
  }
  // See OnNotifyError for why this needs to be the last thing in this
  // function.
  OnNotifyError(kPlatformFailureError);
}

void GpuVideoEncodeAcceleratorHost::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    Client* client) {
  client_ = client;
  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client_));
  Send(new AcceleratedVideoEncoderMsg_Initialize(route_id_,
                                                 input_format,
                                                 input_visible_size,
                                                 output_profile,
                                                 initial_bitrate));
}

void GpuVideoEncodeAcceleratorHost::Encode(
    const scoped_refptr<media::VideoFrame>& frame,
    bool force_keyframe) {
  if (!channel_)
    return;
  if (!base::SharedMemory::IsHandleValid(frame->shared_memory_handle())) {
    DLOG(ERROR) << "Encode(): cannot encode frame not backed by shared memory";
    NotifyError(kPlatformFailureError);
    return;
  }
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(frame->shared_memory_handle());
  if (!base::SharedMemory::IsHandleValid(handle)) {
    DLOG(ERROR) << "Encode(): failed to duplicate buffer handle for GPU "
                   "process";
    NotifyError(kPlatformFailureError);
    return;
  }

  // We assume that planar frame data passed here is packed and contiguous.
  const size_t plane_count = media::VideoFrame::NumPlanes(frame->format());
  size_t frame_size = 0;
  for (size_t i = 0; i < plane_count; ++i) {
    // Cast DCHECK parameters to void* to avoid printing uint8* as a string.
    DCHECK_EQ(reinterpret_cast<void*>(frame->data(i)),
              reinterpret_cast<void*>((frame->data(0) + frame_size)))
        << "plane=" << i;
    frame_size += frame->stride(i) * frame->rows(i);
  }

  Send(new AcceleratedVideoEncoderMsg_Encode(
      route_id_, next_frame_id_, handle, frame_size, force_keyframe));
  frame_map_[next_frame_id_] = frame;

  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_frame_id_ = (next_frame_id_ + 1) & 0x3FFFFFFF;
}

void GpuVideoEncodeAcceleratorHost::UseOutputBitstreamBuffer(
    const media::BitstreamBuffer& buffer) {
  if (!channel_)
    return;
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(buffer.handle());
  if (!base::SharedMemory::IsHandleValid(handle)) {
    DLOG(ERROR) << "UseOutputBitstreamBuffer(): failed to duplicate buffer "
                   "handle for GPU process: buffer.id()=" << buffer.id();
    NotifyError(kPlatformFailureError);
    return;
  }
  Send(new AcceleratedVideoEncoderMsg_UseOutputBitstreamBuffer(
      route_id_, buffer.id(), handle, buffer.size()));
}

void GpuVideoEncodeAcceleratorHost::RequestEncodingParametersChange(
    uint32 bitrate,
    uint32 framerate) {
  Send(new AcceleratedVideoEncoderMsg_RequestEncodingParametersChange(
      route_id_, bitrate, framerate));
}

void GpuVideoEncodeAcceleratorHost::Destroy() {
  Send(new GpuChannelMsg_DestroyVideoEncoder(route_id_));
  delete this;
}

void GpuVideoEncodeAcceleratorHost::NotifyError(Error error) {
  DVLOG(2) << "NotifyError(): error=" << error;
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&media::VideoEncodeAccelerator::Client::NotifyError,
                 client_ptr_factory_->GetWeakPtr(),
                 error));
}

void GpuVideoEncodeAcceleratorHost::OnNotifyInitializeDone() {
  DVLOG(2) << "OnNotifyInitializeDone()";
  if (client_)
    client_->NotifyInitializeDone();
}

void GpuVideoEncodeAcceleratorHost::OnRequireBitstreamBuffers(
    uint32 input_count,
    const gfx::Size& input_coded_size,
    uint32 output_buffer_size) {
  DVLOG(2) << "OnRequireBitstreamBuffers(): input_count=" << input_count
           << ", input_coded_size=" << input_coded_size.ToString()
           << ", output_buffer_size=" << output_buffer_size;
  if (client_) {
    client_->RequireBitstreamBuffers(
        input_count, input_coded_size, output_buffer_size);
  }
}

void GpuVideoEncodeAcceleratorHost::OnNotifyInputDone(int32 frame_id) {
  DVLOG(3) << "OnNotifyInputDone(): frame_id=" << frame_id;
  // Fun-fact: std::hash_map is not spec'd to be re-entrant; since freeing a
  // frame can trigger a further encode to be kicked off and thus an .insert()
  // back into the map, we separate the frame's dtor running from the .erase()
  // running by holding on to the frame temporarily.  This isn't "just
  // theoretical" - Android's std::hash_map crashes if we don't do this.
  scoped_refptr<media::VideoFrame> frame = frame_map_[frame_id];
  if (!frame_map_.erase(frame_id)) {
    DLOG(ERROR) << "OnNotifyInputDone(): "
                   "invalid frame_id=" << frame_id;
    // See OnNotifyError for why this needs to be the last thing in this
    // function.
    OnNotifyError(kPlatformFailureError);
    return;
  }
  frame = NULL;  // Not necessary but nice to be explicit; see fun-fact above.
}

void GpuVideoEncodeAcceleratorHost::OnBitstreamBufferReady(
    int32 bitstream_buffer_id,
    uint32 payload_size,
    bool key_frame) {
  DVLOG(3) << "OnBitstreamBufferReady(): "
              "bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << payload_size
           << ", key_frame=" << key_frame;
  if (client_)
    client_->BitstreamBufferReady(bitstream_buffer_id, payload_size, key_frame);
}

void GpuVideoEncodeAcceleratorHost::OnNotifyError(Error error) {
  DVLOG(2) << "OnNotifyError(): error=" << error;
  if (!client_)
    return;
  client_ptr_factory_.reset();

  // Client::NotifyError() may Destroy() |this|, so calling it needs to be the
  // last thing done on this stack!
  media::VideoEncodeAccelerator::Client* client = NULL;
  std::swap(client_, client);
  client->NotifyError(error);
}

void GpuVideoEncodeAcceleratorHost::Send(IPC::Message* message) {
  if (!channel_) {
    DLOG(ERROR) << "Send(): no channel";
    delete message;
    NotifyError(kPlatformFailureError);
  } else if (!channel_->Send(message)) {
    DLOG(ERROR) << "Send(): sending failed: message->type()="
                << message->type();
    NotifyError(kPlatformFailureError);
  }
}

}  // namespace content
