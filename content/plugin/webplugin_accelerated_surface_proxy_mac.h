// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PLUGIN_WEBPLUGIN_ACCELERATED_SURFACE_PROXY_H_
#define CONTENT_PLUGIN_WEBPLUGIN_ACCELERATED_SURFACE_PROXY_H_
#pragma once

#include "webkit/plugins/npapi/webplugin_accelerated_surface_mac.h"

class WebPluginProxy;
class AcceleratedSurface;

// Out-of-process implementation of WebPluginAcceleratedSurface that proxies
// calls through a WebPluginProxy.
class WebPluginAcceleratedSurfaceProxy
    : public webkit::npapi::WebPluginAcceleratedSurface {
 public:
  // Creates a new WebPluginAcceleratedSurfaceProxy that uses plugin_proxy
  // to proxy calls. plugin_proxy must outlive this object.
  WebPluginAcceleratedSurfaceProxy(WebPluginProxy* plugin_proxy);
  virtual ~WebPluginAcceleratedSurfaceProxy();

  // WebPluginAcceleratedSurface implementation.
  virtual void SetWindowHandle(gfx::PluginWindowHandle window);
  virtual void SetSize(const gfx::Size& size);
  virtual CGLContextObj context();
  virtual void StartDrawing();
  virtual void EndDrawing();

 private:
  WebPluginProxy* plugin_proxy_;  // Weak ref.
  gfx::PluginWindowHandle window_handle_;
  AcceleratedSurface* surface_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginAcceleratedSurfaceProxy);
};

#endif  // CONTENT_PLUGIN_WEBPLUGIN_ACCELERATED_SURFACE_PROXY_H_
