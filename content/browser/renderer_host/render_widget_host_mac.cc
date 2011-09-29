// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"

void RenderWidgetHost::OnMsgPluginFocusChanged(bool focused, int plugin_id) {
  if (view_)
    view_->PluginFocusChanged(focused, plugin_id);
}

void RenderWidgetHost::OnMsgStartPluginIme() {
  if (view_)
    view_->StartPluginIme();
}

void RenderWidgetHost::OnAllocateFakePluginWindowHandle(
    bool opaque,
    bool root,
    gfx::PluginWindowHandle* id) {
  // TODO(kbr): similar potential issue here as in OnMsgCreatePluginContainer.
  // Possibly less of an issue because this is only used for the GPU plugin.
  if (view_) {
    *id = view_->AllocateFakePluginWindowHandle(opaque, root);
  } else {
    NOTIMPLEMENTED();
  }
}

void RenderWidgetHost::OnDestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle id) {
  if (view_) {
    view_->DestroyFakePluginWindowHandle(id);
  } else {
    NOTIMPLEMENTED();
  }
}

void RenderWidgetHost::OnAcceleratedSurfaceSetIOSurface(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    uint64 mach_port) {
  if (view_) {
    view_->AcceleratedSurfaceSetIOSurface(window, width, height, mach_port);
  }
}

void RenderWidgetHost::OnAcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  if (view_) {
    view_->AcceleratedSurfaceSetTransportDIB(window, width, height,
                                             transport_dib);
  }
}

void RenderWidgetHost::OnAcceleratedSurfaceBuffersSwapped(
    gfx::PluginWindowHandle window, uint64 surface_id) {
  if (view_) {
    // This code path could be updated to implement flow control for
    // updating of accelerated plugins as well. However, if we add support
    // for composited plugins then this is not necessary.
    view_->AcceleratedSurfaceBuffersSwapped(window, surface_id,
                                            0, 0, 0);
  }
}
