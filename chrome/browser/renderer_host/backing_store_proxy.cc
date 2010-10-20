// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store_proxy.h"

#include "build/build_config.h"
#include "chrome/browser/gpu_process_host_ui_shim.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/render_messages.h"
#include "gfx/rect.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

BackingStoreProxy::BackingStoreProxy(RenderWidgetHost* widget,
                                     const gfx::Size& size,
                                     GpuProcessHostUIShim* process_shim,
                                     int32 routing_id)
    : BackingStore(widget, size),
      process_shim_(process_shim),
      routing_id_(routing_id),
      waiting_for_paint_ack_(false) {
  process_shim_->AddRoute(routing_id_, this);
}

BackingStoreProxy::~BackingStoreProxy() {
  process_shim_->RemoveRoute(routing_id_);
}

void BackingStoreProxy::PaintToBackingStore(
    RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    bool* painted_synchronously) {
  DCHECK(!waiting_for_paint_ack_);

  base::ProcessId process_id;
#if defined(OS_WIN)
  process_id = ::GetProcessId(process->GetHandle());
#elif defined(OS_POSIX)
  process_id = process->GetHandle();
#endif

  if (process_shim_->Send(new GpuMsg_PaintToBackingStore(
          routing_id_, process_id, bitmap, bitmap_rect, copy_rects))) {
    // Message sent successfully, so the caller can not destroy the
    // TransportDIB. OnDonePaintingToBackingStore will free it later.
    *painted_synchronously = false;
    waiting_for_paint_ack_ = true;
  } else {
    // On error, we're done with the TransportDIB and the caller can free it.
    *painted_synchronously = true;
  }
}

bool BackingStoreProxy::CopyFromBackingStore(const gfx::Rect& rect,
                                             skia::PlatformCanvas* output) {
  NOTIMPLEMENTED();
  return false;
}

void BackingStoreProxy::ScrollBackingStore(int dx, int dy,
                                           const gfx::Rect& clip_rect,
                                           const gfx::Size& view_size) {
  process_shim_->Send(new GpuMsg_ScrollBackingStore(routing_id_, dx, dy,
                                                    clip_rect, view_size));
}

void BackingStoreProxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(BackingStoreProxy, msg)
    IPC_MESSAGE_HANDLER(GpuHostMsg_PaintToBackingStore_ACK,
                        OnPaintToBackingStoreACK)
  IPC_END_MESSAGE_MAP_EX()
}

void BackingStoreProxy::OnChannelConnected(int32 peer_pid) {
}

void BackingStoreProxy::OnChannelError() {
  if (waiting_for_paint_ack_) {
    // If the GPU process dies while painting, the renderer will be waiting for
    // the paint ACK before painting any more. Since no ack is coming, we
    // manually declare that we're done with the transport DIB here so it can
    // continue.
    OnPaintToBackingStoreACK();
  }

  // TODO(brettw): does this mean we aren't getting any more messages and we
  // should delete outselves?
}

void BackingStoreProxy::OnPaintToBackingStoreACK() {
  DCHECK(waiting_for_paint_ack_);
  render_widget_host()->DonePaintingToBackingStore();
  waiting_for_paint_ack_ = false;
}
