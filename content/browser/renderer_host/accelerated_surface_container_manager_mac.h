// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_MANAGER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_MANAGER_MAC_H_

#include <OpenGL/OpenGL.h>
#include <map>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"

namespace gfx {
class Rect;
}

namespace webkit {
namespace npapi {
struct WebPluginGeometry;
}
}

namespace content {

class AcceleratedSurfaceContainerMac;

// Helper class that manages the backing store and on-screen rendering
// of instances of the GPU plugin on the Mac.
class AcceleratedSurfaceContainerManagerMac {
 public:
  AcceleratedSurfaceContainerManagerMac();
  ~AcceleratedSurfaceContainerManagerMac();

  // Allocates a new "fake" PluginWindowHandle, which is used as the
  // key for the other operations.
  gfx::PluginWindowHandle AllocateFakePluginWindowHandle(bool opaque,
                                                         bool root);

  // Destroys a fake PluginWindowHandle and associated storage.
  void DestroyFakePluginWindowHandle(gfx::PluginWindowHandle id);

  // Sets the size and backing store of the plugin instance.  There are two
  // versions: the IOSurface version is used on systems where the IOSurface
  // API is supported (Mac OS X 10.6 and later); the TransportDIB is used on
  // Mac OS X 10.5 and earlier.
  void SetSizeAndIOSurface(gfx::PluginWindowHandle id,
                           int32 width,
                           int32 height,
                           uint64 io_surface_identifier);
  void SetSizeAndTransportDIB(gfx::PluginWindowHandle id,
                              int32 width,
                              int32 height,
                              TransportDIB::Handle transport_dib);

  // Takes an update from WebKit about a plugin's position and size and moves
  // the plugin accordingly.
  void SetPluginContainerGeometry(
      const webkit::npapi::WebPluginGeometry& move);

  // Draws the plugin container associated with the given id into the given
  // OpenGL context, which must already be current.
  void Draw(CGLContextObj context, gfx::PluginWindowHandle id);

  // Causes the next Draw call on each container to trigger a texture upload.
  // Should be called any time the drawing context has changed.
  void ForceTextureReload();

  // Notifies a surface that it has been painted to.
  void SetSurfaceWasPaintedTo(gfx::PluginWindowHandle id,
                              uint64 surface_handle);
  void SetSurfaceWasPaintedTo(gfx::PluginWindowHandle id,
                              uint64 surface_handle,
                              const gfx::Rect& update_rect);

  // Returns if a given surface should be shown.
  bool SurfaceShouldBeVisible(gfx::PluginWindowHandle id) const;
 private:
  uint32 current_id_;

  // Maps a "fake" plugin window handle to the corresponding container.
  AcceleratedSurfaceContainerMac*
      MapIDToContainer(gfx::PluginWindowHandle id) const;

  // A map that associates plugin window handles with their containers.
  typedef std::map<gfx::PluginWindowHandle, AcceleratedSurfaceContainerMac*>
      PluginWindowToContainerMap;
  PluginWindowToContainerMap plugin_window_to_container_map_;

  // Both |plugin_window_to_container_map_| and the
  // AcceleratedSurfaceContainerMac in it are not threadsafe, but accessed from
  // multiple threads. All these accesses are guarded by this lock.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerManagerMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_MANAGER_MAC_H_
