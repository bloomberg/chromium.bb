// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_
#define GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_

#include <d3d11.h>
#include <dcomp.h>
#include <windows.h>

#include "base/memory/weak_ptr.h"
#include "base/win/scoped_comptr.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/service/child_window_win.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {

class GPU_EXPORT DirectCompositionSurfaceWin : public gl::GLSurfaceEGL {
 public:
  DirectCompositionSurfaceWin(
      base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
      HWND parent_window);

  bool InitializeNativeWindow();

  // GLSurfaceEGL implementation.
  using GLSurfaceEGL::Initialize;
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::Size GetSize() override;
  bool IsOffscreen() override;
  void* GetHandle() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  gfx::SwapResult SwapBuffers() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gl::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  bool FlipsVertically() const override;
  bool SupportsPostSubBuffer() override;
  bool OnMakeCurrent(gl::GLContext* context) override;
  bool SupportsSetDrawRectangle() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;

  bool Initialize(std::unique_ptr<gfx::VSyncProvider> vsync_provider);

 protected:
  ~DirectCompositionSurfaceWin() override;

 private:
  struct Overlay {
    Overlay(int z_order,
            gfx::OverlayTransform transform,
            scoped_refptr<gl::GLImage> image,
            gfx::Rect bounds_rect,
            gfx::RectF crop_rect);
    Overlay(const Overlay& overlay);

    ~Overlay();

    int z_order;
    gfx::OverlayTransform transform;
    scoped_refptr<gl::GLImage> image;
    gfx::Rect bounds_rect;
    gfx::RectF crop_rect;
  };

  bool CommitAndClearPendingOverlays();
  void InitializeSurface();
  void ReleaseDrawTexture();

  ChildWindowWin child_window_;

  HWND window_ = nullptr;
  // This is a placeholder surface used when not rendering to the
  // DirectComposition surface.
  EGLSurface default_surface_ = 0;

  // This is the real surface representing the backbuffer. It may be null
  // outside of a BeginDraw/EndDraw pair.
  EGLSurface real_surface_ = 0;
  gfx::Size size_ = gfx::Size(1, 1);
  bool first_swap_ = true;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;
  std::vector<Overlay> pending_overlays_;

  base::win::ScopedComPtr<ID3D11Device> d3d11_device_;
  base::win::ScopedComPtr<IDCompositionDevice2> dcomp_device_;
  base::win::ScopedComPtr<IDCompositionTarget> dcomp_target_;
  base::win::ScopedComPtr<IDCompositionVisual2> visual_;
  base::win::ScopedComPtr<IDCompositionSurface> dcomp_surface_;
  base::win::ScopedComPtr<ID3D11Texture2D> draw_texture_;

  // Keep track of whether the texture has been rendered to, as the first draw
  // to it must overwrite the entire thing.
  bool has_been_rendered_to_ = false;

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionSurfaceWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_
