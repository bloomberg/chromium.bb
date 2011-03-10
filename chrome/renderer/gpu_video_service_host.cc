// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/gpu_video_service_host.h"

#include "chrome/renderer/gpu_video_decoder_host.h"
#include "chrome/renderer/render_thread.h"
#include "content/common/gpu_messages.h"

GpuVideoServiceHost::GpuVideoServiceHost()
    : channel_(NULL),
      next_decoder_host_id_(0) {
}

void GpuVideoServiceHost::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
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

GpuVideoDecoderHost* GpuVideoServiceHost::CreateVideoDecoder(
    int context_route_id) {
  GpuVideoDecoderHost* host = new GpuVideoDecoderHost(&router_, channel_,
                                                      context_route_id,
                                                      next_decoder_host_id_);
  // TODO(hclam): Handle thread safety of incrementing the ID.
  ++next_decoder_host_id_;
  return host;
}
