// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/gl_surface_adapter.h"

#include "base/bind.h"

namespace mus {

GLSurfaceAdapterMus::GLSurfaceAdapterMus(scoped_refptr<gfx::GLSurface> surface)
    : gfx::GLSurfaceAdapter(surface.get()),
      surface_(surface),
      weak_ptr_factory_(this) {}

GLSurfaceAdapterMus::~GLSurfaceAdapterMus() {}

void GLSurfaceAdapterMus::SwapBuffersAsync(
    const GLSurface::SwapCompletionCallback& callback) {
  gfx::GLSurfaceAdapter::SwapBuffersAsync(
      base::Bind(&GLSurfaceAdapterMus::WrappedCallbackForSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void GLSurfaceAdapterMus::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const GLSurface::SwapCompletionCallback& callback) {
  gfx::GLSurfaceAdapter::PostSubBufferAsync(
      x, y, width, height,
      base::Bind(&GLSurfaceAdapterMus::WrappedCallbackForSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void GLSurfaceAdapterMus::CommitOverlayPlanesAsync(
    const GLSurface::SwapCompletionCallback& callback) {
  gfx::GLSurfaceAdapter::CommitOverlayPlanesAsync(
      base::Bind(&GLSurfaceAdapterMus::WrappedCallbackForSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void GLSurfaceAdapterMus::WrappedCallbackForSwapBuffersAsync(
    const gfx::GLSurface::SwapCompletionCallback& original_callback,
    gfx::SwapResult result) {
  if (!adapter_callback_.is_null())
    adapter_callback_.Run(result);
  original_callback.Run(result);
}

void GLSurfaceAdapterMus::SetGpuCompletedSwapBuffersCallback(
    gfx::GLSurface::SwapCompletionCallback callback) {
  adapter_callback_ = std::move(callback);
}

}  // namespace mus
