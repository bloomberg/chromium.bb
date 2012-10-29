// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/accelerated_surface_container_manager_mac.h"

#include <OpenGL/gl.h>

#include "base/logging.h"
#include "content/browser/renderer_host/accelerated_surface_container_mac.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "ui/gfx/rect.h"

namespace content {

AcceleratedSurfaceContainerManagerMac::AcceleratedSurfaceContainerManagerMac()
    : current_id_(0) {
}

AcceleratedSurfaceContainerManagerMac::
    ~AcceleratedSurfaceContainerManagerMac() {}

gfx::PluginWindowHandle
AcceleratedSurfaceContainerManagerMac::AllocateFakePluginWindowHandle(
    bool opaque, bool root) {
  base::AutoLock lock(lock_);

  AcceleratedSurfaceContainerMac* container =
      new AcceleratedSurfaceContainerMac(this, opaque);
  gfx::PluginWindowHandle res =
      static_cast<gfx::PluginWindowHandle>(++current_id_);
  plugin_window_to_container_map_.insert(std::make_pair(res, container));
  DCHECK(!root);
  return res;
}

void AcceleratedSurfaceContainerManagerMac::DestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle id) {
  base::AutoLock lock(lock_);

  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  if (container)
    delete container;
  plugin_window_to_container_map_.erase(id);
}

void AcceleratedSurfaceContainerManagerMac::SetSizeAndIOSurface(
    gfx::PluginWindowHandle id,
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
  base::AutoLock lock(lock_);

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
  base::AutoLock lock(lock_);

  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  if (container)
    container->SetSizeAndTransportDIB(width, height, transport_dib);
}

void AcceleratedSurfaceContainerManagerMac::SetPluginContainerGeometry(
    const webkit::npapi::WebPluginGeometry& move) {
  base::AutoLock lock(lock_);

  AcceleratedSurfaceContainerMac* container = MapIDToContainer(move.window);
  if (container)
    container->SetGeometry(move);
}

void AcceleratedSurfaceContainerManagerMac::Draw(CGLContextObj context,
                                                 gfx::PluginWindowHandle id) {
  base::AutoLock lock(lock_);

  glColorMask(true, true, true, true);
  // Should match the clear color of RenderWidgetHostViewMac.
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  // TODO(thakis): Clearing the whole color buffer is wasteful, since most of
  // it is overwritten by the surface.
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
  base::AutoLock lock(lock_);

  for (PluginWindowToContainerMap::const_iterator i =
          plugin_window_to_container_map_.begin();
       i != plugin_window_to_container_map_.end(); ++i) {
    AcceleratedSurfaceContainerMac* container = i->second;
    container->ForceTextureReload();
  }
}

void AcceleratedSurfaceContainerManagerMac::SetSurfaceWasPaintedTo(
    gfx::PluginWindowHandle id, uint64 surface_handle) {
  base::AutoLock lock(lock_);

  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  if (container)
    container->set_was_painted_to(surface_handle);
}

void AcceleratedSurfaceContainerManagerMac::SetSurfaceWasPaintedTo(
    gfx::PluginWindowHandle id,
    uint64 surface_handle,
    const gfx::Rect& update_rect) {
  base::AutoLock lock(lock_);

  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  if (container)
    container->set_was_painted_to(surface_handle, update_rect);
}

bool AcceleratedSurfaceContainerManagerMac::SurfaceShouldBeVisible(
    gfx::PluginWindowHandle id) const {
  base::AutoLock lock(lock_);

  AcceleratedSurfaceContainerMac* container = MapIDToContainer(id);
  return container && container->ShouldBeVisible();
}

AcceleratedSurfaceContainerMac*
    AcceleratedSurfaceContainerManagerMac::MapIDToContainer(
        gfx::PluginWindowHandle id) const {
  PluginWindowToContainerMap::const_iterator i =
      plugin_window_to_container_map_.find(id);
  if (i != plugin_window_to_container_map_.end())
    return i->second;

  LOG(ERROR) << "Request for plugin container for unknown window id " << id;

  return NULL;
}

}  // namespace content
