// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_CLIENT_LAYER_TREE_FRAME_SINK_H_
#define COMPONENTS_VIZ_CLIENT_CLIENT_LAYER_TREE_FRAME_SINK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/ipc/compositor_frame_sink.mojom.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace viz {

class LocalSurfaceIdProvider;
class SharedBitmapManager;

class ClientLayerTreeFrameSink : public cc::LayerTreeFrameSink,
                                 public cc::mojom::CompositorFrameSinkClient,
                                 public cc::ExternalBeginFrameSourceClient {
 public:
  ClientLayerTreeFrameSink(
      scoped_refptr<ContextProvider> context_provider,
      scoped_refptr<ContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      SharedBitmapManager* shared_bitmap_manager,
      std::unique_ptr<cc::SyntheticBeginFrameSource>
          synthetic_begin_frame_source,
      cc::mojom::CompositorFrameSinkPtrInfo compositor_frame_sink_info,
      cc::mojom::CompositorFrameSinkClientRequest client_request,
      std::unique_ptr<LocalSurfaceIdProvider> local_surface_id_provider,
      bool enable_surface_synchronization);

  ClientLayerTreeFrameSink(
      scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
      std::unique_ptr<cc::SyntheticBeginFrameSource>
          synthetic_begin_frame_source,
      cc::mojom::CompositorFrameSinkPtrInfo compositor_frame_sink_info,
      cc::mojom::CompositorFrameSinkClientRequest client_request,
      std::unique_ptr<LocalSurfaceIdProvider> local_surface_id_provider,
      bool enable_surface_synchronization);

  ~ClientLayerTreeFrameSink() override;

  base::WeakPtr<ClientLayerTreeFrameSink> GetWeakPtr();

  // cc::LayerTreeFrameSink implementation.
  bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SetLocalSurfaceId(const LocalSurfaceId& local_surface_id) override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const cc::BeginFrameAck& ack) override;

 private:
  // cc::mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_args) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;

  // cc::ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  static void OnMojoConnectionError(uint32_t custom_reason,
                                    const std::string& description);

  LocalSurfaceId local_surface_id_;
  std::unique_ptr<LocalSurfaceIdProvider> local_surface_id_provider_;
  std::unique_ptr<cc::ExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source_;
  cc::mojom::CompositorFrameSinkPtrInfo compositor_frame_sink_info_;
  cc::mojom::CompositorFrameSinkClientRequest client_request_;
  cc::mojom::CompositorFrameSinkPtr compositor_frame_sink_;
  mojo::Binding<cc::mojom::CompositorFrameSinkClient> client_binding_;
  THREAD_CHECKER(thread_checker_);
  const bool enable_surface_synchronization_;

  base::WeakPtrFactory<ClientLayerTreeFrameSink> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientLayerTreeFrameSink);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_CLIENT_LAYER_TREE_FRAME_SINK_H_
