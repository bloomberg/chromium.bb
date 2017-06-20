// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_
#define COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "cc/output/layer_tree_frame_sink_client.h"
#include "cc/resources/release_callback.h"
#include "components/exo/surface_observer.h"

namespace cc {
class LayerTreeFrameSink;
}

namespace exo {
class Surface;

// This class talks to MojoCompositorFrameSink and keeps track of references to
// the contents of Buffers. It's keeped alive by references from
// release_callbacks_. It's destroyed when its owning Surface is destroyed and
// the last outstanding release callback is called.
class LayerTreeFrameSinkHolder : public cc::LayerTreeFrameSinkClient,
                                 public SurfaceObserver {
 public:
  LayerTreeFrameSinkHolder(Surface* surface,
                           std::unique_ptr<cc::LayerTreeFrameSink> frame_sink);
  ~LayerTreeFrameSinkHolder() override;

  bool HasReleaseCallbackForResource(cc::ResourceId id);
  void SetResourceReleaseCallback(cc::ResourceId id,
                                  const cc::ReleaseCallback& callback);

  cc::LayerTreeFrameSink* GetLayerTreeFrameSink() { return frame_sink_.get(); }

  base::WeakPtr<LayerTreeFrameSinkHolder> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(cc::BeginFrameSource* source) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void SetTreeActivationCallback(const base::Closure& callback) override {}
  void DidReceiveCompositorFrameAck() override;
  void DidLoseLayerTreeFrameSink() override {}
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

 private:
  // A collection of callbacks used to release resources.
  using ResourceReleaseCallbackMap = base::flat_map<int, cc::ReleaseCallback>;
  ResourceReleaseCallbackMap release_callbacks_;

  Surface* surface_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;

  base::WeakPtrFactory<LayerTreeFrameSinkHolder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeFrameSinkHolder);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_
