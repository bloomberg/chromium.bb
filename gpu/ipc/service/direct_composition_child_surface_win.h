// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_
#define GPU_IPC_SERVICE_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_

#include <d3d11.h>
#include <dcomp.h>
#include <windows.h>

#include "base/win/scoped_comptr.h"
#include "gpu/gpu_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {

class GPU_EXPORT DirectCompositionChildSurfaceWin : public gl::GLSurfaceEGL {
 public:
  DirectCompositionChildSurfaceWin(const gfx::Size& size,
                                   bool has_alpha,
                                   bool enable_dc_layers);

  // GLSurfaceEGL implementation.
  using GLSurfaceEGL::Initialize;
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::Size GetSize() override;
  bool IsOffscreen() override;
  void* GetHandle() override;
  gfx::SwapResult SwapBuffers() override;
  bool FlipsVertically() const override;
  bool SupportsPostSubBuffer() override;
  bool OnMakeCurrent(gl::GLContext* context) override;
  bool SupportsDCLayers() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  gfx::Vector2d GetDrawOffset() const override;

  const base::win::ScopedComPtr<IDCompositionSurface>& dcomp_surface() const {
    return dcomp_surface_;
  }

  const base::win::ScopedComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

 protected:
  ~DirectCompositionChildSurfaceWin() override;

 private:
  void ReleaseCurrentSurface();
  void InitializeSurface();
  // Release the texture that's currently being drawn to. If will_discard is
  // true then the surface should be discarded without swapping any contents
  // to it.
  void ReleaseDrawTexture(bool will_discard);

  // This is a placeholder surface used when not rendering to the
  // DirectComposition surface.
  EGLSurface default_surface_ = 0;

  // This is the real surface representing the backbuffer. It may be null
  // outside of a BeginDraw/EndDraw pair.
  EGLSurface real_surface_ = 0;
  bool first_swap_ = true;
  const gfx::Size size_;
  const bool has_alpha_;
  const bool enable_dc_layers_;
  gfx::Rect swap_rect_;
  gfx::Vector2d draw_offset_;

  base::win::ScopedComPtr<ID3D11Device> d3d11_device_;
  base::win::ScopedComPtr<IDCompositionDevice2> dcomp_device_;
  base::win::ScopedComPtr<IDCompositionSurface> dcomp_surface_;
  base::win::ScopedComPtr<IDXGISwapChain1> swap_chain_;
  base::win::ScopedComPtr<ID3D11Texture2D> draw_texture_;

  // Keep track of whether the texture has been rendered to, as the first draw
  // to it must overwrite the entire thing.
  bool has_been_rendered_to_ = false;

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionChildSurfaceWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_
