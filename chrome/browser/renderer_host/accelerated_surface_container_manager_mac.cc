// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/accelerated_surface_container_manager_mac.h"

#include "base/logging.h"
#include "chrome/browser/renderer_host/accelerated_surface_container_mac.h"
#include "webkit/glue/plugins/webplugin.h"

AcceleratedSurfaceContainerManagerMac::AcceleratedSurfaceContainerManagerMac()
    : current_id_(0),
      root_container_(NULL) {
}

gfx::PluginWindowHandle
AcceleratedSurfaceContainerManagerMac::AllocateFakePluginWindowHandle(
    bool opaque, bool root) {
  AcceleratedSurfaceContainerMac* container =
      new AcceleratedSurfaceContainerMac(this, opaque);
  gfx::PluginWindowHandle res =
      static_cast<gfx::PluginWindowHandle>(++current_id_);
  plugin_window_to_container_map_.insert(std::make_pair(res, container));
  if (root) {
    root_container_ = container;
  }
  return res;
}

void AcceleratedSurfaceContainerManagerMac::DestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle id) {
  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  if (container) {
    if (container == root_container_)
      root_container_ = NULL;
    delete container;
  }
  plugin_window_to_container_map_.erase(id);
}

bool AcceleratedSurfaceContainerManagerMac::IsRootContainer(
    gfx::PluginWindowHandle id) {
  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  return root_container_ == container;
}

void AcceleratedSurfaceContainerManagerMac::SetSizeAndIOSurface(
    gfx::PluginWindowHandle id,
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  if (container) {
    container->SetSizeAndIOSurface(width, height, io_surface_identifier);
  }
}

void AcceleratedSurfaceContainerManagerMac::SetSizeAndTransportDIB(
    gfx::PluginWindowHandle id,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  if (container)
    container->SetSizeAndTransportDIB(width, height, transport_dib);
}

void AcceleratedSurfaceContainerManagerMac::SetPluginContainerGeometry(
    const webkit_glue::WebPluginGeometry& move) {
  AcceleratedSurfaceContainerMac* container = MapIDToContainer(move.window);
  if (container)
    container->SetGeometry(move);
}

void AcceleratedSurfaceContainerManagerMac::Draw(CGLContextObj context,
                                                 gfx::PluginWindowHandle id,
                                                 bool draw_root_container) {
  glColorMask(true, true, true, true);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  CHECK(container);
  container->Draw(context);

  // Unbind any texture from the texture target to ensure that the
  // next time through we will have to re-bind the texture and thereby
  // pick up modifications from the other process.
  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  glBindTexture(target, 0);

  glFlush();
}

void AcceleratedSurfaceContainerManagerMac::ForceTextureReload() {
  for (PluginWindowToContainerMap::const_iterator i =
          plugin_window_to_container_map_.begin();
       i != plugin_window_to_container_map_.end(); ++i) {
    AcceleratedSurfaceContainerMac* container = i->second;
    container->ForceTextureReload();
  }
}

AcceleratedSurfaceContainerMac*
    AcceleratedSurfaceContainerManagerMac::MapIDToContainer(
        gfx::PluginWindowHandle id) {
  PluginWindowToContainerMap::const_iterator i =
      plugin_window_to_container_map_.find(id);
  if (i != plugin_window_to_container_map_.end())
    return i->second;

  LOG(ERROR) << "Request for plugin container for unknown window id " << id;

  return NULL;
}
