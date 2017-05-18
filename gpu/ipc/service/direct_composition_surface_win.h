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
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/service/child_window_win.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {

class DCLayerTree;

class GPU_EXPORT DirectCompositionSurfaceWin : public gl::GLSurfaceEGL {
 public:
  DirectCompositionSurfaceWin(
      std::unique_ptr<gfx::VSyncProvider> vsync_provider,
      base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
      HWND parent_window);

  // Returns true if there's an output on the current adapter that can
  // use overlays.
  static bool AreOverlaysSupported();

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
  bool SetEnableDCLayers(bool enable) override;
  bool FlipsVertically() const override;
  bool SupportsPostSubBuffer() override;
  bool OnMakeCurrent(gl::GLContext* context) override;
  bool SupportsDCLayers() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  gfx::Vector2d GetDrawOffset() const override;
  void WaitForSnapshotRendering() override;

  // This schedules an overlay plane to be displayed on the next SwapBuffers
  // or PostSubBuffer call. Overlay planes must be scheduled before every swap
  // to remain in the layer tree. This surface's backbuffer doesn't have to be
  // scheduled with ScheduleDCLayer, as it's automatically placed in the layer
  // tree at z-order 0.
  bool ScheduleDCLayer(const ui::DCRendererLayerParams& params) override;

  const base::win::ScopedComPtr<IDCompositionSurface>& dcomp_surface() const {
    return dcomp_surface_;
  }

  scoped_refptr<base::TaskRunner> GetWindowTaskRunnerForTesting();
  const base::win::ScopedComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

  base::win::ScopedComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  const GpuDriverBugWorkarounds& workarounds() const { return workarounds_; }

 protected:
  ~DirectCompositionSurfaceWin() override;

 private:
  void ReleaseCurrentSurface();
  void InitializeSurface();
  // Release the texture that's currently being drawn to. If will_discard is
  // true then the surface should be discarded without swapping any contents
  // to it.
  void ReleaseDrawTexture(bool will_discard);

  ChildWindowWin child_window_;

  GpuDriverBugWorkarounds workarounds_;

  HWND window_ = nullptr;
  // This is a placeholder surface used when not rendering to the
  // DirectComposition surface.
  EGLSurface default_surface_ = 0;

  // This is the real surface representing the backbuffer. It may be null
  // outside of a BeginDraw/EndDraw pair.
  EGLSurface real_surface_ = 0;
  gfx::Size size_ = gfx::Size(1, 1);
  bool first_swap_ = true;
  bool enable_dc_layers_ = false;
  bool has_alpha_ = true;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;
  gfx::Rect swap_rect_;
  std::unique_ptr<DCLayerTree> layer_tree_;
  gfx::Vector2d draw_offset_;

  base::win::ScopedComPtr<ID3D11Device> d3d11_device_;
  base::win::ScopedComPtr<IDCompositionDevice2> dcomp_device_;
  base::win::ScopedComPtr<IDCompositionSurface> dcomp_surface_;
  base::win::ScopedComPtr<IDXGISwapChain1> swap_chain_;
  base::win::ScopedComPtr<ID3D11Texture2D> draw_texture_;

  // Keep track of whether the texture has been rendered to, as the first draw
  // to it must overwrite the entire thing.
  bool has_been_rendered_to_ = false;

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionSurfaceWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_
