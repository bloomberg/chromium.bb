// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include <memory>

#include "gpu/ipc/service/child_window_surface_win.h"
#include "gpu/ipc/service/direct_composition_surface_win.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "gpu/ipc/service/switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/vsync_provider_win.h"

namespace gpu {

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  scoped_refptr<gl::GLSurface> surface;
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2 &&
      gl::GLSurfaceEGL::IsDirectCompositionSupported()) {
    std::unique_ptr<gfx::VSyncProvider> vsync_provider(
        new gl::VSyncProviderWin(surface_handle));
    if (base::FeatureList::IsEnabled(switches::kDirectCompositionOverlays)) {
      scoped_refptr<DirectCompositionSurfaceWin> egl_surface =
          make_scoped_refptr(
              new DirectCompositionSurfaceWin(delegate, surface_handle));
      if (!egl_surface->Initialize(std::move(vsync_provider)))
        return nullptr;
      surface = egl_surface;
    } else {
      scoped_refptr<ChildWindowSurfaceWin> egl_surface = make_scoped_refptr(
          new ChildWindowSurfaceWin(delegate, surface_handle));
      if (!egl_surface->Initialize(std::move(vsync_provider)))
        return nullptr;
      surface = egl_surface;
    }
  } else {
    surface = gl::init::CreateViewGLSurface(surface_handle);
    if (!surface)
      return nullptr;
  }

  return scoped_refptr<gl::GLSurface>(
      new PassThroughImageTransportSurface(delegate, surface.get()));
}

}  // namespace gpu
