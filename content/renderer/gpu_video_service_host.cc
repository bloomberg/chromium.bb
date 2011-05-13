// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu_video_service_host.h"

#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/gpu_video_decode_accelerator_host.h"
#include "content/renderer/render_thread.h"
#include "media/video/video_decode_accelerator.h"

GpuVideoServiceHost::GpuVideoServiceHost()
    : channel_(NULL),
      next_decoder_host_id_(0) {
}

GpuVideoServiceHost::~GpuVideoServiceHost() {
}

void GpuVideoServiceHost::OnFilterAdded(IPC::Channel* channel) {
  base::Closure on_initialized;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!channel_);
    channel_ = channel;
    on_initialized = on_initialized_;
  }
  if (!on_initialized.is_null())
    on_initialized.Run();
}

void GpuVideoServiceHost::OnFilterRemoved() {
  // TODO(hclam): Implement.
}

void GpuVideoServiceHost::OnChannelClosing() {
  // TODO(hclam): Implement.
}

bool GpuVideoServiceHost::OnMessageReceived(const IPC::Message& msg) {
  switch (msg.type()) {
    case AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed::ID:
    case AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers::ID:
    case AcceleratedVideoDecoderHostMsg_CreateDone::ID:
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

void GpuVideoServiceHost::SetOnInitialized(
    const base::Closure& on_initialized) {
  IPC::Channel* channel;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(on_initialized_.is_null());
    on_initialized_ = on_initialized;
    channel = channel_;
  }
  if (channel)
    on_initialized.Run();
}

GpuVideoDecoderHost* GpuVideoServiceHost::CreateVideoDecoder(
    int context_route_id) {
  // TODO(vrk): Delete all references to GpuVideoDecoder (deprecated).
  return NULL;
}

GpuVideoDecodeAcceleratorHost* GpuVideoServiceHost::CreateVideoAccelerator(
    media::VideoDecodeAccelerator::Client* client) {
  base::AutoLock auto_lock(lock_);
  DCHECK(channel_);
  return new GpuVideoDecodeAcceleratorHost(
      &router_, channel_, next_decoder_host_id_++, client);
}
