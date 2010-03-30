// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/gpu_view_host.h"

#include "chrome/browser/gpu_process_host_ui_shim.h"
#include "chrome/browser/renderer_host/backing_store_proxy.h"
#include "chrome/browser/renderer_host/video_layer_proxy.h"
#include "chrome/common/gpu_messages.h"

GpuViewHost::GpuViewHost(RenderWidgetHost* widget, GpuNativeWindowHandle parent)
    : widget_(widget),
      process_shim_(GpuProcessHostUIShim::Get()),
      routing_id_(0) {
  if (!process_shim_) {
    // TODO(brettw) handle error.
    return;
  }
  routing_id_ = process_shim_->NewRenderWidgetHostView(parent);
}

GpuViewHost::~GpuViewHost() {
}

BackingStore* GpuViewHost::CreateBackingStore(const gfx::Size& size) {
  int32 backing_store_routing_id = process_shim_->GetNextRoutingId();
  BackingStoreProxy* result =
      new BackingStoreProxy(widget_, size,
                            process_shim_, backing_store_routing_id);
  process_shim_->Send(new GpuMsg_NewBackingStore(routing_id_,
                                                 backing_store_routing_id,
                                                 size));
  return result;
}

VideoLayer* GpuViewHost::CreateVideoLayer(const gfx::Size& size) {
  int32 video_layer_routing_id = process_shim_->GetNextRoutingId();
  VideoLayerProxy* result =
      new VideoLayerProxy(widget_, size,
                          process_shim_, video_layer_routing_id);
  process_shim_->Send(new GpuMsg_NewVideoLayer(routing_id_,
                                               video_layer_routing_id,
                                               size));
  return result;
}

void GpuViewHost::OnWindowPainted() {
  process_shim_->Send(new GpuMsg_WindowPainted(routing_id_));
}
