// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_GPU_PLUGIN_CONTAINER_MAC_H_
#define CHROME_BROWSER_RENDERER_HOST_GPU_PLUGIN_CONTAINER_MAC_H_

// The "GPU plugin" is currently implemented as a special kind of
// NPAPI plugin to provide high-performance on-screen 3D rendering for
// Pepper 3D.
//
// On Windows and X11 platforms the GPU plugin relies on cross-process
// parenting of windows, which is not supported via any public APIs in
// the Mac OS X window system.
//
// To achieve full hardware acceleration we use the new IOSurface APIs
// introduced in Mac OS X 10.6. The GPU plugin's process produces an
// IOSurface and renders into it using OpenGL. It uses the
// IOSurfaceGetID and IOSurfaceLookup APIs to pass a reference to this
// surface to the browser process for on-screen rendering. The GPU
// plugin essentially looks like a windowless plugin; the browser
// process gets all of the mouse events, because the plugin process
// does not have an on-screen window.
//
// This class encapsulates some of the management of these data
// structures, in conjunction with the AcceleratedSurfaceContainerManagerMac.

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/OpenGL.h>

#include "app/surface/transport_dib.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"

namespace webkit_glue {
struct WebPluginGeometry;
}

class AcceleratedSurfaceContainerManagerMac;

class AcceleratedSurfaceContainerMac {
 public:
  AcceleratedSurfaceContainerMac(
      AcceleratedSurfaceContainerManagerMac* manager,
      bool opaque);
  virtual ~AcceleratedSurfaceContainerMac();

  // Sets the backing store and size of this accelerated surface container.
  // There are two versions: the IOSurface version is used on systems where the
  // IOSurface API is supported (Mac OS X 10.6 and later); the TransportDIB is
  // used on Mac OS X 10.5 and earlier.
  void SetSizeAndIOSurface(int32 width,
                           int32 height,
                           uint64 io_surface_identifier);
  void SetSizeAndTransportDIB(int32 width,
                              int32 height,
                              TransportDIB::Handle transport_dib);

  // Tells the accelerated surface container that it has moved relative to the
  // origin of the window, for example because of a scroll event.
  void MoveTo(const webkit_glue::WebPluginGeometry& geom);

  // Draws this accelerated surface's contents, texture mapped onto a quad in
  // the given OpenGL context. TODO(kbr): figure out and define exactly how the
  // coordinate system will work out.
  void Draw(CGLContextObj context);

  // Causes the next Draw call to trigger a texture upload. Should be called any
  // time the drawing context has changed.
  void ForceTextureReload() { texture_needs_upload_ = true; }

 private:
  // The manager of this accelerated surface container.
  AcceleratedSurfaceContainerManagerMac* manager_;

  // Whether this accelerated surface's content is supposed to be opaque.
  bool opaque_;

  // The x and y coordinates of the plugin window on the web page.
  int x_;
  int y_;

  // The IOSurfaceRef, if any, that has been handed from the GPU
  // plugin process back to the browser process for drawing.
  // This is held as a CFTypeRef because we can't refer to the
  // IOSurfaceRef type when building on 10.5.
  CFTypeRef surface_;

  // The TransportDIB which is used in pre-10.6 systems where the IOSurface
  // API is not supported.  This is a weak reference to the actual TransportDIB
  // whic is owned by the GPU process.
  scoped_ptr<TransportDIB> transport_dib_;

  // The width and height of the surface.
  int32 width_;
  int32 height_;

  // The clip rectangle, relative to the (x_, y_) origin.
  gfx::Rect clipRect_;

  // The "live" OpenGL texture referring to this IOSurfaceRef. Note
  // that per the CGLTexImageIOSurface2D API we do not need to
  // explicitly update this texture's contents once created. All we
  // need to do is ensure it is re-bound before attempting to draw
  // with it.
  GLuint texture_;

  // True if we need to upload the texture again during the next draw.
  bool texture_needs_upload_;

  // Releases the IOSurface reference, if any, retained by this object.
  void ReleaseIOSurface();

  // Enqueue our texture for later deletion.
  void EnqueueTextureForDeletion();

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerMac);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_GPU_PLUGIN_CONTAINER_MAC_H_

