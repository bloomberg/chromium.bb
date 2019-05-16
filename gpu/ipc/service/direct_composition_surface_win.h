// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_
#define GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "gpu/ipc/service/child_window_win.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {
class GLSurfacePresentationHelper;
class VSyncThreadWin;
}

namespace gpu {
class DCLayerTree;
class DirectCompositionChildSurfaceWin;

class GPU_IPC_SERVICE_EXPORT DirectCompositionSurfaceWin
    : public gl::GLSurfaceEGL {
 public:
  using VSyncCallback =
      base::RepeatingCallback<void(base::TimeTicks, base::TimeDelta)>;

  // Common overlay formats that we're interested in. Must match the
  // OverlayFormat enum in //tools/metrics/histograms/enums.xml. Mapped to
  // corresponding DXGI formats in DirectCompositionSurfaceWin.
  enum class OverlayFormat {
    kBGRA = 0,
    kYUY2 = 1,
    kNV12 = 2,
    kMaxValue = kNV12
  };

  struct Settings {
    bool disable_nv12_dynamic_textures = false;
    bool disable_larger_than_screen_overlays = false;
  };

  DirectCompositionSurfaceWin(
      std::unique_ptr<gfx::VSyncProvider> vsync_provider,
      VSyncCallback vsync_callback,
      HWND parent_window,
      const DirectCompositionSurfaceWin::Settings& settings);

  // Returns true if direct composition is supported.  We prefer to use direct
  // composition event without hardware overlays, because it allows us to bypass
  // blitting by DWM to the window redirection surface by using a flip mode swap
  // chain.  Overridden with --disable-direct-composition.
  static bool IsDirectCompositionSupported();

  // Returns true if hardware overlays are supported, and DirectComposition
  // surface and layers should be used.  Overridden with
  // --enable-direct-composition-layers and --disable-direct-composition-layers.
  static bool AreOverlaysSupported();

  // After this is called, hardware overlay support is disabled during the
  // current GPU process' lifetime.
  static void DisableOverlays();

  // Returns true if scaled hardware overlays are supported.
  static bool AreScaledOverlaysSupported();

  // Returns preferred overlay format set when detecting hardware overlay
  // support.
  static OverlayFormat GetOverlayFormatUsed();
  static DXGI_FORMAT GetOverlayDxgiFormatUsed();

  // Returns monitor size.
  static gfx::Size GetOverlayMonitorSize();

  // Returns if the given format is supported for hardware overlays.
  // Also, if |supports_scaling| is set to true, SCALING mode is supported for
  // the given format; otherwise, only DIRECT mode is supported.
  static bool SupportsOverlayFormat(OverlayFormat format,
                                    bool* supports_scaling);

  // Returns true if there is an HDR capable display connected.
  static bool IsHDRSupported();

  static void SetScaledOverlaysSupportedForTesting(bool value);

  static void SetPreferNV12OverlaysForTesting();

  bool InitializeNativeWindow();

  // GLSurfaceEGL implementation.
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::Size GetSize() override;
  bool IsOffscreen() override;
  void* GetHandle() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback) override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  void SetVSyncEnabled(bool enabled) override;
  bool SetEnableDCLayers(bool enable) override;
  bool FlipsVertically() const override;
  bool SupportsPresentationCallback() override;
  bool SupportsPostSubBuffer() override;
  bool OnMakeCurrent(gl::GLContext* context) override;
  bool SupportsDCLayers() const override;
  bool UseOverlaysForVideo() const override;
  bool SupportsProtectedVideo() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  gfx::Vector2d GetDrawOffset() const override;
  bool SupportsGpuVSync() const override;
  void SetGpuVSyncEnabled(bool enabled) override;

  // This schedules an overlay plane to be displayed on the next SwapBuffers
  // or PostSubBuffer call. Overlay planes must be scheduled before every swap
  // to remain in the layer tree. This surface's backbuffer doesn't have to be
  // scheduled with ScheduleDCLayer, as it's automatically placed in the layer
  // tree at z-order 0.
  bool ScheduleDCLayer(const ui::DCRendererLayerParams& params) override;

  HWND window() const { return window_; }

  scoped_refptr<base::TaskRunner> GetWindowTaskRunnerForTesting();

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetBackbufferSwapChainForTesting()
      const;

 protected:
  ~DirectCompositionSurfaceWin() override;

 private:
  HWND window_ = nullptr;
  ChildWindowWin child_window_;

  std::unique_ptr<gl::VSyncThreadWin> vsync_thread_;
  scoped_refptr<DirectCompositionChildSurfaceWin> root_surface_;
  std::unique_ptr<DCLayerTree> layer_tree_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;
  const VSyncCallback vsync_callback_;
  std::unique_ptr<gl::GLSurfacePresentationHelper> presentation_helper_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionSurfaceWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DIRECT_COMPOSITION_SURFACE_WIN_H_
