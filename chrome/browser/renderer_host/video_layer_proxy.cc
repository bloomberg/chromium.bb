// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/video_layer_proxy.h"

#include "chrome/browser/gpu_process_host_ui_shim.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/gpu_messages.h"
#include "gfx/rect.h"

VideoLayerProxy::VideoLayerProxy(RenderWidgetHost* widget,
                                 const gfx::Size& size,
                                 GpuProcessHostUIShim* process_shim,
                                 int32 routing_id)
    : VideoLayer(widget, size),
      process_shim_(process_shim),
      routing_id_(routing_id) {
  process_shim_->AddRoute(routing_id_, this);
}

VideoLayerProxy::~VideoLayerProxy() {
  process_shim_->RemoveRoute(routing_id_);
}

void VideoLayerProxy::CopyTransportDIB(RenderProcessHost* process,
                                       TransportDIB::Id bitmap,
                                       const gfx::Rect& bitmap_rect) {
  base::ProcessId process_id;
#if defined(OS_WIN)
  process_id = ::GetProcessId(process->GetHandle());
#elif defined(OS_POSIX)
  process_id = process->GetHandle();
#endif

  if (process_shim_->Send(new GpuMsg_PaintToVideoLayer(
          routing_id_, process_id, bitmap, bitmap_rect))) {
  } else {
    // TODO(scherkus): what to do ?!?!
  }
}

void VideoLayerProxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(VideoLayerProxy, msg)
    IPC_MESSAGE_HANDLER(GpuHostMsg_PaintToVideoLayer_ACK,
                        OnPaintToVideoLayerACK)
  IPC_END_MESSAGE_MAP_EX()
}

void VideoLayerProxy::OnChannelConnected(int32 peer_pid) {
}

void VideoLayerProxy::OnChannelError() {
}

void VideoLayerProxy::OnPaintToVideoLayerACK() {
  // TODO(scherkus): we may not need to ACK video layer updates at all.
  NOTIMPLEMENTED();
}
