// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_
#define GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_

#include "base/memory/weak_ptr.h"
#include "gpu/ipc/service/child_window_win.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

#include <windows.h>

namespace gpu {

class DirectCompositionSurfaceWin : public gl::GLSurfaceEGL {
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
  bool SupportsPostSubBuffer() override;

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

  ChildWindowWin child_window_;

  HWND window_;
  EGLSurface surface_;
  gfx::Size size_;
  bool first_swap_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;
  std::vector<Overlay> pending_overlays_;

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionSurfaceWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_
