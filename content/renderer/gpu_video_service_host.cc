// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu_video_service_host.h"

#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/gpu_video_decoder_host.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/video_decode_accelerator_host.h"
#include "media/video/video_decode_accelerator.h"

using media::VideoDecodeAccelerator;

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
    case GpuVideoDecoderHostMsg_CreateVideoDecoderDone::ID:
    case GpuVideoDecoderHostMsg_InitializeACK::ID:
    case GpuVideoDecoderHostMsg_DestroyACK::ID:
    case GpuVideoDecoderHostMsg_FlushACK::ID:
    case GpuVideoDecoderHostMsg_PrerollDone::ID:
    case GpuVideoDecoderHostMsg_EmptyThisBufferACK::ID:
    case GpuVideoDecoderHostMsg_EmptyThisBufferDone::ID:
    case GpuVideoDecoderHostMsg_ConsumeVideoFrame::ID:
    case GpuVideoDecoderHostMsg_AllocateVideoFrames::ID:
    case GpuVideoDecoderHostMsg_ReleaseAllVideoFrames::ID:
      if (!router_.RouteMessage(msg)) {
        LOG(ERROR) << "GpuVideoDecoderHostMsg cannot be dispatched.";
      }
      return true;
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
  base::AutoLock auto_lock(lock_);
  DCHECK(channel_);
  return new GpuVideoDecoderHost(
      &router_, channel_, context_route_id, ++next_decoder_host_id_);
}

VideoDecodeAccelerator* GpuVideoServiceHost::CreateVideoAccelerator() {
  base::AutoLock auto_lock(lock_);
   DCHECK(channel_);
  return new VideoDecodeAcceleratorHost(
      &router_, channel_, next_decoder_host_id_);
}
