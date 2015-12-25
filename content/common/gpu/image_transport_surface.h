// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_MACOSX)
struct AcceleratedSurfaceMsg_BufferPresented_Params;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
#endif

namespace gfx {
class GLSurface;
}

namespace gpu {
class PreemptionFlag;
namespace gles2 {
class GLES2Decoder;
}
}

namespace content {
class GpuChannelManager;
class GpuCommandBufferStub;

// The GPU process is agnostic as to how it displays results. On some platforms
// it renders directly to window. On others it renders offscreen and transports
// the results to the browser process to display. This file provides a simple
// framework for making the offscreen path seem more like the onscreen path.
//
// The ImageTransportSurface class defines an simple interface for events that
// should be responded to. The factory returns an offscreen surface that looks
// a lot like an onscreen surface to the GPU process.
//
// The ImageTransportSurfaceHelper provides some glue to the outside world:
// making sure outside events reach the ImageTransportSurface and
// allowing the ImageTransportSurface to send events to the outside world.

class ImageTransportSurface {
 public:
  ImageTransportSurface();

#if defined(OS_MACOSX)
  virtual void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) = 0;
#endif
  virtual void SetLatencyInfo(
      const std::vector<ui::LatencyInfo>& latency_info) = 0;

  // Creates a surface with the given attributes.
  static scoped_refptr<gfx::GLSurface> CreateSurface(
      GpuChannelManager* manager,
      GpuCommandBufferStub* stub,
      const gfx::GLSurfaceHandle& handle);

#if defined(OS_MACOSX)
  CONTENT_EXPORT static void SetAllowOSMesaForTesting(bool allow);
#endif

  virtual gfx::Size GetSize() = 0;

 protected:
  virtual ~ImageTransportSurface();

 private:
  // Creates the appropriate native surface depending on the GL implementation.
  // This will be implemented separately by each platform.
  //
  // This will not be called for texture transport surfaces which are
  // cross-platform. The platform implementation should only create the
  // surface and should not initialize it. On failure, a null scoped_refptr
  // should be returned.
  static scoped_refptr<gfx::GLSurface> CreateNativeSurface(
      GpuChannelManager* manager,
      GpuCommandBufferStub* stub,
      const gfx::GLSurfaceHandle& handle);

  DISALLOW_COPY_AND_ASSIGN(ImageTransportSurface);
};

class ImageTransportHelper
    : public IPC::Listener,
      public base::SupportsWeakPtr<ImageTransportHelper> {
 public:
  // Takes weak pointers to objects that outlive the helper.
  ImageTransportHelper(ImageTransportSurface* surface,
                       GpuChannelManager* manager,
                       GpuCommandBufferStub* stub,
                       gfx::PluginWindowHandle handle);
  ~ImageTransportHelper() override;

  bool Initialize();

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  // Helper send functions. Caller fills in the surface specific params
  // like size and surface id. The helper fills in the rest.
#if defined(OS_MACOSX)
  void SendAcceleratedSurfaceBuffersSwapped(
      GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params);
#endif

  // Make the surface's context current.
  bool MakeCurrent();

  // Set the default swap interval on the surface.
  static void SetSwapInterval(gfx::GLContext* context);

  GpuChannelManager* manager() const { return manager_; }
  GpuCommandBufferStub* stub() const { return stub_.get(); }

 private:
  gpu::gles2::GLES2Decoder* Decoder();

  // IPC::Message handlers.
#if defined(OS_MACOSX)
  void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params);
#endif

  // Backbuffer resize callback.
  void Resize(gfx::Size size, float scale_factor);

  void SetLatencyInfo(const std::vector<ui::LatencyInfo>& latency_info);

  // Weak pointers that point to objects that outlive this helper.
  ImageTransportSurface* surface_;
  GpuChannelManager* manager_;

  base::WeakPtr<GpuCommandBufferStub> stub_;
  int32_t route_id_;
  gfx::PluginWindowHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(ImageTransportHelper);
};

// An implementation of ImageTransportSurface that implements GLSurface through
// GLSurfaceAdapter, thereby forwarding GLSurface methods through to it.
class PassThroughImageTransportSurface
    : public gfx::GLSurfaceAdapter,
      public ImageTransportSurface {
 public:
  PassThroughImageTransportSurface(GpuChannelManager* manager,
                                   GpuCommandBufferStub* stub,
                                   gfx::GLSurface* surface);

  // GLSurface implementation.
  bool Initialize() override;
  void Destroy() override;
  gfx::SwapResult SwapBuffers() override;
  void SwapBuffersAsync(const SwapCompletionCallback& callback) override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          const SwapCompletionCallback& callback) override;
  gfx::SwapResult CommitOverlayPlanes() override;
  void CommitOverlayPlanesAsync(
      const SwapCompletionCallback& callback) override;
  bool OnMakeCurrent(gfx::GLContext* context) override;

  // ImageTransportSurface implementation.
#if defined(OS_MACOSX)
  void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) override;
#endif
  gfx::Size GetSize() override;
  void SetLatencyInfo(
      const std::vector<ui::LatencyInfo>& latency_info) override;

 protected:
  ~PassThroughImageTransportSurface() override;

  // If updated vsync parameters can be determined, send this information to
  // the browser.
  virtual void SendVSyncUpdateIfAvailable();

  scoped_ptr<std::vector<ui::LatencyInfo>> StartSwapBuffers();
  void FinishSwapBuffers(scoped_ptr<std::vector<ui::LatencyInfo>> latency_info,
                         gfx::SwapResult result);
  void FinishSwapBuffersAsync(
      scoped_ptr<std::vector<ui::LatencyInfo>> latency_info,
      GLSurface::SwapCompletionCallback callback,
      gfx::SwapResult result);

  ImageTransportHelper* GetHelper() { return helper_.get(); }

 private:
  scoped_ptr<ImageTransportHelper> helper_;
  bool did_set_swap_interval_;
  std::vector<ui::LatencyInfo> latency_info_;
  base::WeakPtrFactory<PassThroughImageTransportSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PassThroughImageTransportSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_H_
