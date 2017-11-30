// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_DELEGATE_H_
#define GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_DELEGATE_H_

#include "base/callback.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/swap_result.h"

#if defined(OS_MACOSX)
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/mac/io_surface.h"
#endif

namespace IPC {
class MessageFilter;
}

namespace gfx {
struct PresentationFeedback;
}

namespace gpu {
struct GpuPreferences;

namespace gles2 {
class FeatureInfo;
}

struct GPU_EXPORT SwapBuffersCompleteParams {
  SwapBuffersCompleteParams();
  SwapBuffersCompleteParams(SwapBuffersCompleteParams&& other);
  ~SwapBuffersCompleteParams();

  SwapBuffersCompleteParams& operator=(SwapBuffersCompleteParams&& other);

#if defined(OS_MACOSX)
  // Mac-specific parameters used to present CALayers hosted in the GPU process.
  // TODO(ccameron): Remove these parameters once the CALayer tree is hosted in
  // the browser process.
  // https://crbug.com/604052
  // Only one of ca_context_id or io_surface may be non-0.
  CAContextID ca_context_id = 0;
  bool fullscreen_low_power_ca_context_valid = false;
  CAContextID fullscreen_low_power_ca_context_id = 0;
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface;
  gfx::Size pixel_size;
  float scale_factor = 1.f;
  gpu::TextureInUseResponses in_use_responses;
#endif
  gfx::SwapResponse response;
};

class GPU_EXPORT ImageTransportSurfaceDelegate {
 public:
#if defined(OS_WIN)
  // Tells the delegate that a child window was created with the provided
  // SurfaceHandle.
  virtual void DidCreateAcceleratedSurfaceChildWindow(
      SurfaceHandle parent_window,
      SurfaceHandle child_window) = 0;
#endif

  // Tells the delegate that SwapBuffers returned.
  virtual void DidSwapBuffersComplete(SwapBuffersCompleteParams params) = 0;

  // Returns the features available for the ContextGroup.
  virtual const gles2::FeatureInfo* GetFeatureInfo() const = 0;

  virtual const GpuPreferences& GetGpuPreferences() const = 0;

  virtual void SetSnapshotRequestedCallback(const base::Closure& callback) = 0;

  // Informs the delegate about updated vsync parameters.
  virtual void UpdateVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) = 0;

  // Tells the delegate a buffer has been presented.
  virtual void BufferPresented(uint64_t swap_id,
                               const gfx::PresentationFeedback& feedback) = 0;
  // Add IPC message filter.
  virtual void AddFilter(IPC::MessageFilter* message_filter) = 0;
  // Gets route ID for sending / receiving IPC messages.
  virtual int32_t GetRouteID() const = 0;

 protected:
  virtual ~ImageTransportSurfaceDelegate() {}
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_DELEGATE_H_
