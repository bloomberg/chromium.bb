// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/gpu_view_host_win.h"

#include "chrome/browser/gpu_process_host.h"
#include "chrome/browser/renderer_host/backing_store_proxy.h"
#include "chrome/common/gpu_messages.h"

GpuViewHostWin::GpuViewHostWin(RenderWidgetHostViewWin* view,
                               HWND parent)
    : view_(view),
      process_(GpuProcessHost::Get()),
      routing_id_(0) {
  if (!process_) {
    // TODO(brettw) handle error.
    return;
  }
  routing_id_ = process_->NewRenderWidgetHostView(
      gfx::IdFromNativeView(parent));
}

GpuViewHostWin::~GpuViewHostWin() {
}

BackingStore* GpuViewHostWin::CreateBackingStore(
    RenderWidgetHost* widget,
    const gfx::Size& size) {
  int32 backing_store_routing_id = process_->GetNextRoutingId();
  process_->Send(new GpuMsg_NewBackingStore(routing_id_,
                                            backing_store_routing_id,
                                            size));
  return new BackingStoreProxy(widget, size,
                               process_, backing_store_routing_id);
}
