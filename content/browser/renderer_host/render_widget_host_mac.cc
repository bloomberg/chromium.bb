// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"

namespace content {

void RenderWidgetHostImpl::OnPluginFocusChanged(bool focused, int plugin_id) {
  if (view_)
    view_->PluginFocusChanged(focused, plugin_id);
}

void RenderWidgetHostImpl::OnStartPluginIme() {
  if (view_)
    view_->StartPluginIme();
}

void RenderWidgetHostImpl::OnAllocateFakePluginWindowHandle(
    bool opaque,
    bool root,
    gfx::PluginWindowHandle* id) {
  // TODO(kbr): similar potential issue here as in OnCreatePluginContainer.
  // Possibly less of an issue because this is only used for the GPU plugin.
  if (view_) {
    *id = view_->AllocateFakePluginWindowHandle(opaque, root);
  } else {
    NOTIMPLEMENTED();
  }
}

void RenderWidgetHostImpl::OnDestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle id) {
  if (view_) {
    view_->DestroyFakePluginWindowHandle(id);
  } else {
    NOTIMPLEMENTED();
  }
}

void RenderWidgetHostImpl::OnAcceleratedSurfaceSetIOSurface(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    uint64 mach_port) {
  if (view_) {
    view_->AcceleratedSurfaceSetIOSurface(window, width, height, mach_port);
  }
}

void RenderWidgetHostImpl::OnAcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  if (view_) {
    view_->AcceleratedSurfaceSetTransportDIB(window, width, height,
                                             transport_dib);
  }
}

void RenderWidgetHostImpl::OnAcceleratedSurfaceBuffersSwapped(
    gfx::PluginWindowHandle window, uint64 surface_handle) {
  if (view_) {
    // This code path could be updated to implement flow control for
    // updating of accelerated plugins as well. However, if we add support
    // for composited plugins then this is not necessary.
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
    params.window = window;
    params.surface_handle = surface_handle;
    view_->AcceleratedSurfaceBuffersSwapped(params, 0);
  }
}

}  // namespace content
