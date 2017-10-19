// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "ui/compositor/compositor.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class SingleThreadTaskGraphRunner;
}

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace ui {
class ContextProviderCommandBuffer;
}

namespace content {

// A replacement for GpuProcessTransportFactory to be used when running viz. In
// this configuration the display compositor is located in the viz process
// instead of in the browser process. Any interaction with the display
// compositor must happen over IPC.
class VizProcessTransportFactory : public ui::ContextFactory,
                                   public ui::ContextFactoryPrivate,
                                   public ImageTransportFactory {
 public:
  VizProcessTransportFactory(
      gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory,
      scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner);
  ~VizProcessTransportFactory() override;

  // Connects HostFrameSinkManager to FrameSinkManagerImpl in viz process.
  void ConnectHostFrameSinkManager();

  // ui::ContextFactory implementation.
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<viz::ContextProvider> SharedMainThreadContextProvider()
      override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  double GetRefreshRate() const override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  const viz::ResourceSettings& GetResourceSettings() const override;
  void AddObserver(ui::ContextFactoryObserver* observer) override;
  void RemoveObserver(ui::ContextFactoryObserver* observer) override;

  // ui::ContextFactoryPrivate implementation.
  std::unique_ptr<ui::Reflector> CreateReflector(ui::Compositor* source,
                                                 ui::Layer* target) override;
  void RemoveReflector(ui::Reflector* reflector) override;
  viz::FrameSinkId AllocateFrameSinkId() override;
  viz::HostFrameSinkManager* GetHostFrameSinkManager() override;
  void SetDisplayVisible(ui::Compositor* compositor, bool visible) override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;
  void SetDisplayColorSpace(ui::Compositor* compositor,
                            const gfx::ColorSpace& blending_color_space,
                            const gfx::ColorSpace& output_color_space) override;
  void SetAuthoritativeVSyncInterval(ui::Compositor* compositor,
                                     base::TimeDelta interval) override;
  void SetDisplayVSyncParameters(ui::Compositor* compositor,
                                 base::TimeTicks timebase,
                                 base::TimeDelta interval) override;
  void IssueExternalBeginFrame(ui::Compositor* compositor,
                               const viz::BeginFrameArgs& args) override;
  void SetOutputIsSecure(ui::Compositor* compositor, bool secure) override;
  viz::FrameSinkManagerImpl* GetFrameSinkManager() override;

  // ImageTransportFactory implementation.
  ui::ContextFactory* GetContextFactory() override;
  ui::ContextFactoryPrivate* GetContextFactoryPrivate() override;
  viz::GLHelper* GetGLHelper() override;
#if defined(OS_MACOSX)
  void SetCompositorSuspendedForRecycle(ui::Compositor* compositor,
                                        bool suspended) override;
#endif

 private:
  struct CompositorData {
    CompositorData();
    CompositorData(CompositorData&& other);
    ~CompositorData();
    CompositorData& operator=(CompositorData&& other);

    // Privileged interface that controls the display for a root
    // CompositorFrameSink.
    viz::mojom::DisplayPrivateAssociatedPtr display_private;

   private:
    DISALLOW_COPY_AND_ASSIGN(CompositorData);
  };

  // Finishes creation of LayerTreeFrameSink after GPU channel has been
  // established.
  void CreateLayerTreeFrameSinkForGpuChannel(
      base::WeakPtr<ui::Compositor> compositor_weak_ptr,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  // Creates a ContextProvider for a compositor. Will also create
  // |shared_worker_context_provider_| if it doesn't exist.
  scoped_refptr<ui::ContextProviderCommandBuffer> CreateContextProvider(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);

  gpu::GpuChannelEstablishFactory* const gpu_channel_establish_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner_;

  base::flat_map<ui::Compositor*, CompositorData> compositor_data_map_;

  // TODO(kylechar): Call OnContextLost() on observers when GPU crashes.
  base::ObserverList<ui::ContextFactoryObserver> observer_list_;

  scoped_refptr<ui::ContextProviderCommandBuffer>
      shared_worker_context_provider_;

  viz::FrameSinkIdAllocator frame_sink_id_allocator_;
  std::unique_ptr<cc::SingleThreadTaskGraphRunner> task_graph_runner_;
  const viz::RendererSettings renderer_settings_;

  base::WeakPtrFactory<VizProcessTransportFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VizProcessTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_
