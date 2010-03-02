// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_video_layer_glx.h"

#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_thread.h"

GpuVideoLayerGLX::GpuVideoLayerGLX(GpuViewX* view,
                                   GpuThread* gpu_thread,
                                   int32 routing_id,
                                   const gfx::Size& size)
    : view_(view),
      gpu_thread_(gpu_thread),
      routing_id_(routing_id),
      size_(size) {
  gpu_thread_->AddRoute(routing_id_, this);
}

GpuVideoLayerGLX::~GpuVideoLayerGLX() {
  gpu_thread_->RemoveRoute(routing_id_);
}

void GpuVideoLayerGLX::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GpuVideoLayerGLX, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_PaintToVideoLayer, OnPaintToVideoLayer)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuVideoLayerGLX::OnChannelConnected(int32 peer_pid) {
}

void GpuVideoLayerGLX::OnChannelError() {
  // FIXME(brettw) does this mean we aren't getting any more messages and we
  // should delete outselves?
  NOTIMPLEMENTED();
}

void GpuVideoLayerGLX::OnPaintToVideoLayer(base::ProcessId source_process_id,
                                           TransportDIB::Id id,
                                           const gfx::Rect& bitmap_rect) {
  // TODO(scherkus): implement GPU video layer.
  NOTIMPLEMENTED();

  // TODO(scherkus): we may not need to ACK video layer updates at all.
  gpu_thread_->Send(new GpuHostMsg_PaintToVideoLayer_ACK(routing_id_));
}
