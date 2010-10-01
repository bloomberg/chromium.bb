// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <OpenGL/OpenGL.h>

#include "chrome/plugin/webplugin_accelerated_surface_proxy_mac.h"

#include "app/surface/accelerated_surface_mac.h"
#include "app/surface/transport_dib.h"
#include "chrome/plugin/webplugin_proxy.h"

WebPluginAcceleratedSurfaceProxy::WebPluginAcceleratedSurfaceProxy(
    WebPluginProxy* plugin_proxy) : plugin_proxy_(plugin_proxy) {
  surface_ = new AcceleratedSurface;
  surface_->Initialize(NULL, true);

  // Only used for 10.5 support, but harmless on 10.6+.
  surface_->SetTransportDIBAllocAndFree(
      NewCallback(plugin_proxy_, &WebPluginProxy::AllocSurfaceDIB),
      NewCallback(plugin_proxy_, &WebPluginProxy::FreeSurfaceDIB));
}

WebPluginAcceleratedSurfaceProxy::~WebPluginAcceleratedSurfaceProxy() {
  if (surface_) {
    surface_->Destroy();
    delete surface_;
    surface_ = NULL;
  }
}

void WebPluginAcceleratedSurfaceProxy::SetWindowHandle(
    gfx::PluginWindowHandle window) {
  window_handle_ = window;
}

void WebPluginAcceleratedSurfaceProxy::SetSize(const gfx::Size& size) {
  uint64 io_surface_id = surface_->SetSurfaceSize(size);
  if (io_surface_id) {
    plugin_proxy_->SetAcceleratedSurface(window_handle_, size, io_surface_id);
  } else {
    TransportDIB::Handle transport_dib = surface_->SetTransportDIBSize(size);
    if (TransportDIB::is_valid(transport_dib)) {
      plugin_proxy_->SetAcceleratedDIB(window_handle_, size, transport_dib);
    }
  }
}

CGLContextObj WebPluginAcceleratedSurfaceProxy::context() {
  return surface_->context();
}

void WebPluginAcceleratedSurfaceProxy::StartDrawing() {
  surface_->MakeCurrent();
  surface_->Clear(gfx::Rect(surface_->GetSize()));
}

void WebPluginAcceleratedSurfaceProxy::EndDrawing() {
  surface_->SwapBuffers();
  plugin_proxy_->AcceleratedFrameBuffersDidSwap(window_handle_);
}
