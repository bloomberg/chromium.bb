// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_

#include <stdint.h>

#include <map>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "ui/compositor/compositor.h"

namespace base {
class SimpleThread;
class Thread;
}

namespace cc {
class SingleThreadTaskGraphRunner;
class SoftwareOutputDevice;
class SurfaceManager;
}

namespace content {
class BrowserCompositorOutputSurface;
class CompositorSwapClient;
class ContextProviderCommandBuffer;
class OutputDeviceBacking;
class ReflectorImpl;
class WebGraphicsContext3DCommandBufferImpl;

class GpuProcessTransportFactory
    : public ui::ContextFactory,
      public ImageTransportFactory {
 public:
  GpuProcessTransportFactory();

  ~GpuProcessTransportFactory() override;

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
  CreateOffscreenCommandBufferContext();

  // ui::ContextFactory implementation.
  void CreateOutputSurface(base::WeakPtr<ui::Compositor> compositor) override;
  scoped_ptr<ui::Reflector> CreateReflector(ui::Compositor* source,
                                            ui::Layer* target) override;
  void RemoveReflector(ui::Reflector* reflector) override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  bool DoesCreateTestContexts() override;
  uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                 gfx::BufferUsage usage) override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator() override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;

  // ImageTransportFactory implementation.
  ui::ContextFactory* GetContextFactory() override;
  cc::SurfaceManager* GetSurfaceManager() override;
  GLHelper* GetGLHelper() override;
  void AddObserver(ImageTransportFactoryObserver* observer) override;
  void RemoveObserver(ImageTransportFactoryObserver* observer) override;
#if defined(OS_MACOSX)
  void OnGpuSwapBuffersCompleted(
      int surface_id,
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result) override;
  void SetCompositorSuspendedForRecycle(ui::Compositor* compositor,
                                        bool suspended) override;
  bool SurfaceShouldNotShowFramesAfterSuspendForRecycle(
      int surface_id) const override;
#endif

 private:
  struct PerCompositorData;

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor);
  scoped_ptr<cc::SoftwareOutputDevice> CreateSoftwareOutputDevice(
      ui::Compositor* compositor);
  void EstablishedGpuChannel(base::WeakPtr<ui::Compositor> compositor,
                             bool create_gpu_output_surface,
                             int num_attempts);
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateContextCommon(
      scoped_refptr<GpuChannelHost> gpu_channel_host,
      int surface_id);

  void OnLostMainThreadSharedContextInsideCallback();
  void OnLostMainThreadSharedContext();

  typedef std::map<ui::Compositor*, PerCompositorData*> PerCompositorDataMap;
  PerCompositorDataMap per_compositor_data_;
  scoped_refptr<ContextProviderCommandBuffer> shared_main_thread_contexts_;
  scoped_ptr<GLHelper> gl_helper_;
  base::ObserverList<ImageTransportFactoryObserver> observer_list_;
  scoped_ptr<cc::SurfaceManager> surface_manager_;
  uint32_t next_surface_id_namespace_;
  scoped_ptr<cc::SingleThreadTaskGraphRunner> task_graph_runner_;
  scoped_refptr<ContextProviderCommandBuffer> shared_worker_context_provider_;

#if defined(OS_WIN)
  scoped_ptr<OutputDeviceBacking> software_backing_;
#endif

  // The contents of this map and its methods may only be used on the compositor
  // thread.
  IDMap<BrowserCompositorOutputSurface> output_surface_map_;

  base::WeakPtrFactory<GpuProcessTransportFactory> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_
