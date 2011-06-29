// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_video_service_host.h"

#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/gpu/gpu_video_decode_accelerator_host.h"
#include "content/renderer/render_thread.h"
#include "media/video/video_decode_accelerator.h"

GpuVideoServiceHost::GpuVideoServiceHost()
    : channel_(NULL),
      next_decoder_host_id_(0) {
  DCHECK(RenderThread::current());
  DCHECK_EQ(RenderThread::current()->message_loop(), MessageLoop::current());
}

GpuVideoServiceHost::~GpuVideoServiceHost() {
}

void GpuVideoServiceHost::set_channel(IPC::SyncChannel* channel) {
  DCHECK(CalledOnValidThread());
  DCHECK(!channel_);
  channel_ = channel;
  if (!on_initialized_.is_null())
    on_initialized_.Run();
}

bool GpuVideoServiceHost::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(CalledOnValidThread());
  if (!channel_)
    return false;
  switch (msg.type()) {
    case AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed::ID:
    case AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers::ID:
    case AcceleratedVideoDecoderHostMsg_CreateDone::ID:
    case AcceleratedVideoDecoderHostMsg_InitializeDone::ID:
    case AcceleratedVideoDecoderHostMsg_DismissPictureBuffer::ID:
    case AcceleratedVideoDecoderHostMsg_PictureReady::ID:
    case AcceleratedVideoDecoderHostMsg_FlushDone::ID:
    case AcceleratedVideoDecoderHostMsg_AbortDone::ID:
    case AcceleratedVideoDecoderHostMsg_EndOfStream::ID:
    case AcceleratedVideoDecoderHostMsg_ErrorNotification::ID:
      if (router_.RouteMessage(msg))
        return true;
      LOG(ERROR) << "AcceleratedVideoDecoderHostMsg cannot be dispatched.";
    default:
      return false;
  }
}

void GpuVideoServiceHost::OnChannelError() {
  DCHECK(CalledOnValidThread());
  channel_ = NULL;
}

void GpuVideoServiceHost::SetOnInitialized(
    const base::Closure& on_initialized) {
  DCHECK(CalledOnValidThread());
  DCHECK(on_initialized_.is_null());
  on_initialized_ = on_initialized;
  if (channel_)
    on_initialized.Run();
}

GpuVideoDecodeAcceleratorHost* GpuVideoServiceHost::CreateVideoAccelerator(
    media::VideoDecodeAccelerator::Client* client,
    int32 command_buffer_route_id,
    gpu::CommandBufferHelper* cmd_buffer_helper) {
  DCHECK(CalledOnValidThread());
  DCHECK(channel_);
  return new GpuVideoDecodeAcceleratorHost(
      &router_, channel_, next_decoder_host_id_++,
      command_buffer_route_id, cmd_buffer_helper, client);
}
