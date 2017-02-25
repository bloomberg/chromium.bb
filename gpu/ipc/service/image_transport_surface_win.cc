// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include <memory>

#include "base/win/windows_version.h"
#include "gpu/ipc/service/child_window_surface_win.h"
#include "gpu/ipc/service/direct_composition_surface_win.h"
#include "gpu/ipc/service/gpu_vsync_provider_win.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "gpu/ipc/service/switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/vsync_provider_win.h"

namespace gpu {

namespace {
bool IsGpuVSyncSignalSupported() {
  // TODO(stanisc): http://crbug.com/467617 Limit to Windows 8+ for now because
  // of locking issue caused by waiting for VSync on Win7.
  return base::win::GetVersion() >= base::win::VERSION_WIN8 &&
         base::FeatureList::IsEnabled(features::kD3DVsync);
}

}  // namespace

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  scoped_refptr<gl::GLSurface> surface;
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2) {
    std::unique_ptr<gfx::VSyncProvider> vsync_provider;

    if (IsGpuVSyncSignalSupported())
      vsync_provider.reset(new GpuVSyncProviderWin(delegate, surface_handle));
    else
      vsync_provider.reset(new gl::VSyncProviderWin(surface_handle));

    if (gl::GLSurfaceEGL::IsDirectCompositionSupported()) {
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
      surface = gl::init::CreateNativeViewGLSurfaceEGL(
          surface_handle, std::move(vsync_provider));
      if (!surface)
        return nullptr;
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
