// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <OpenGL/OpenGL.h>

#include "content/plugin/webplugin_accelerated_surface_proxy_mac.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/plugin/webplugin_proxy.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/surface/accelerated_surface_mac.h"
#include "ui/gfx/surface/io_surface_support_mac.h"
#include "ui/gfx/surface/transport_dib.h"

WebPluginAcceleratedSurfaceProxy* WebPluginAcceleratedSurfaceProxy::Create(
    WebPluginProxy* plugin_proxy,
    gfx::GpuPreference gpu_preference) {
  bool composited = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableCompositedCoreAnimationPlugins);

  // Require IOSurface support for drawing Core Animation plugins.
  if (composited && !IOSurfaceSupport::Initialize())
    return NULL;

  AcceleratedSurface* surface = new AcceleratedSurface;
  // It's possible for OpenGL to fail to initialize (e.g., if an incompatible
  // mode is forced via flags), so handle that gracefully.
  if (!surface->Initialize(NULL, true, gpu_preference)) {
    delete surface;
    surface = NULL;
    if (composited)
      return NULL;
  }

  if (!composited && surface) {
    // Only used for 10.5 support, but harmless on 10.6+.
    surface->SetTransportDIBAllocAndFree(
        base::Bind(&WebPluginProxy::AllocSurfaceDIB,
                   base::Unretained(plugin_proxy)),
        base::Bind(&WebPluginProxy::FreeSurfaceDIB,
                   base::Unretained(plugin_proxy)));
  }

  return new WebPluginAcceleratedSurfaceProxy(
      plugin_proxy, surface, composited);
}

WebPluginAcceleratedSurfaceProxy::WebPluginAcceleratedSurfaceProxy(
    WebPluginProxy* plugin_proxy,
    AcceleratedSurface* surface,
    bool composited)
        : plugin_proxy_(plugin_proxy),
          surface_(surface),
          composited_(composited) {
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

bool WebPluginAcceleratedSurfaceProxy::IsComposited() {
  return composited_;
}

void WebPluginAcceleratedSurfaceProxy::SetSize(const gfx::Size& size) {
  if (!surface_)
    return;

  if (composited_) {
    uint32 io_surface_id = surface_->SetSurfaceSize(size);
    // If allocation fails for some reason, still inform the plugin proxy.
    plugin_proxy_->AcceleratedPluginAllocatedIOSurface(
        size.width(), size.height(), io_surface_id);
  } else {
    uint32 io_surface_id = surface_->SetSurfaceSize(size);
    if (io_surface_id) {
      plugin_proxy_->SetAcceleratedSurface(window_handle_, size, io_surface_id);
    } else {
      TransportDIB::Handle transport_dib = surface_->SetTransportDIBSize(size);
      if (TransportDIB::is_valid_handle(transport_dib)) {
        plugin_proxy_->SetAcceleratedDIB(window_handle_, size, transport_dib);
      }
    }
  }
}

CGLContextObj WebPluginAcceleratedSurfaceProxy::context() {
  return surface_ ? surface_->context() : NULL;
}

void WebPluginAcceleratedSurfaceProxy::StartDrawing() {
  if (!surface_)
    return;

  surface_->MakeCurrent();
  surface_->Clear(gfx::Rect(surface_->GetSize()));
}

void WebPluginAcceleratedSurfaceProxy::EndDrawing() {
  if (!surface_)
    return;

  surface_->SwapBuffers();
  if (composited_) {
    plugin_proxy_->AcceleratedPluginSwappedIOSurface();
  } else {
    plugin_proxy_->AcceleratedFrameBuffersDidSwap(
        window_handle_, surface_->GetSurfaceId());
  }
}
