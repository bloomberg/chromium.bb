// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "base/logging.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  if (gl::GetGLImplementation() == gl::kGLImplementationOSMesaGL) {
    return gl::InitializeGLSurfaceWithFormat(
        new gl::GLSurfaceOSMesa(format, gfx::Size(1, 1)), format);
  }

  DCHECK(gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
         gl::GetGLImplementation() == gl::kGLImplementationStubGL);
  return new gl::GLSurfaceStub;
}

}  // namespace gpu
