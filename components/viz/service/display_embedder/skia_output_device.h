// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/src/gpu/GrSemaphore.h"
#include "ui/gfx/swap_result.h"
#include "ui/latency/latency_tracker.h"

class SkSurface;

namespace gfx {
class ColorSpace;
class Rect;
class Size;
struct PresentationFeedback;
}  // namespace gfx

namespace gpu {
class MemoryTracker;
class MemoryTypeTracker;
}  // namespace gpu

namespace viz {

class SkiaOutputDevice {
 public:
  // A helper class for defining a BeginPaint() and EndPaint() scope.
  class ScopedPaint {
   public:
    explicit ScopedPaint(SkiaOutputDevice* device);
    ~ScopedPaint();

    SkSurface* sk_surface() const { return sk_surface_; }

    std::vector<GrBackendSemaphore> TakeEndPaintSemaphores() {
      std::vector<GrBackendSemaphore> semaphores;
      semaphores.swap(end_semaphores_);
      return semaphores;
    }

   private:
    std::vector<GrBackendSemaphore> end_semaphores_;
    SkiaOutputDevice* const device_;
    SkSurface* const sk_surface_;

    DISALLOW_COPY_AND_ASSIGN(ScopedPaint);
  };

  using BufferPresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size)>;
  SkiaOutputDevice(
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  virtual ~SkiaOutputDevice();

  // Changes the size of draw surface and invalidates it's contents.
  virtual bool Reshape(const gfx::Size& size,
                       float device_scale_factor,
                       const gfx::ColorSpace& color_space,
                       gfx::BufferFormat format,
                       gfx::OverlayTransform transform) = 0;

  // Presents the back buffer.
  virtual void SwapBuffers(BufferPresentedCallback feedback,
                           std::vector<ui::LatencyInfo> latency_info) = 0;
  virtual void PostSubBuffer(const gfx::Rect& rect,
                             BufferPresentedCallback feedback,
                             std::vector<ui::LatencyInfo> latency_info);
  virtual void CommitOverlayPlanes(BufferPresentedCallback feedback,
                                   std::vector<ui::LatencyInfo> latency_info);

  // Set the rectangle that will be drawn into on the surface.
  virtual void SetDrawRectangle(const gfx::Rect& draw_rectangle);

  virtual void SetGpuVSyncEnabled(bool enabled);

  // Whether the output device's primary plane is an overlay. This returns true
  // is the SchedulePrimaryPlane function is implemented.
  virtual bool IsPrimaryPlaneOverlay() const;
  // Schedule the output device's back buffer as an overlay plane. The scheduled
  // primary plane will be on screen when SwapBuffers() or PostSubBuffer() is
  // called.
  virtual void SchedulePrimaryPlane(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane);

  // Schedule overlays which will be on screen when SwapBuffers() or
  // PostSubBuffer() is called.
  virtual void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays);

#if defined(OS_WIN)
  virtual void SetEnableDCLayers(bool enabled);
#endif

  const OutputSurface::Capabilities& capabilities() const {
    return capabilities_;
  }

  // EnsureBackbuffer called when output surface is visible and may be drawn to.
  // DiscardBackbuffer called when output surface is hidden and will not be
  // drawn to. Default no-op.
  virtual void EnsureBackbuffer();
  virtual void DiscardBackbuffer();

  bool is_emulated_rgbx() const { return is_emulated_rgbx_; }

 protected:
  // Only valid between StartSwapBuffers and FinishSwapBuffers.
  class SwapInfo {
   public:
    SwapInfo(uint64_t swap_id, BufferPresentedCallback feedback);
    SwapInfo(SwapInfo&& other);
    ~SwapInfo();
    const gpu::SwapBuffersCompleteParams& Complete(gfx::SwapResult result);
    void CallFeedback();

   private:
    BufferPresentedCallback feedback_;
    gpu::SwapBuffersCompleteParams params_;
  };

  // Begin paint the back buffer.
  virtual SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) = 0;

  // End paint the back buffer.
  virtual void EndPaint() = 0;

  // Helper method for SwapBuffers() and PostSubBuffer(). It should be called
  // at the beginning of SwapBuffers() and PostSubBuffer() implementations
  void StartSwapBuffers(BufferPresentedCallback feedback);

  // Helper method for SwapBuffers() and PostSubBuffer(). It should be called
  // at the end of SwapBuffers() and PostSubBuffer() implementations
  void FinishSwapBuffers(gfx::SwapResult result,
                         const gfx::Size& size,
                         std::vector<ui::LatencyInfo> latency_info);

  OutputSurface::Capabilities capabilities_;

  uint64_t swap_id_ = 0;
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;

  base::queue<SwapInfo> pending_swaps_;

  ui::LatencyTracker latency_tracker_;

  // RGBX format is emulated with RGBA.
  bool is_emulated_rgbx_ = false;

  std::unique_ptr<gpu::MemoryTypeTracker> memory_type_tracker_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDevice);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
