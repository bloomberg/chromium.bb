// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_COMPOSITOR_FRAME_SINK_HOLDER_H_
#define COMPONENTS_EXO_COMPOSITOR_FRAME_SINK_HOLDER_H_

#include <list>
#include <map>
#include <memory>

#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "cc/resources/release_callback.h"
#include "cc/resources/transferable_resource.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/exo/compositor_frame_sink.h"
#include "components/exo/surface_observer.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace exo {
class Surface;

// This class talks to MojoCompositorFrameSink and keeps track of references to
// the contents of Buffers. It's keeped alive by references from
// release_callbacks_. It's destroyed when its owning Surface is destroyed and
// the last outstanding release callback is called.
class CompositorFrameSinkHolder
    : public base::RefCounted<CompositorFrameSinkHolder>,
      public cc::ExternalBeginFrameSourceClient,
      public cc::mojom::MojoCompositorFrameSinkClient,
      public SurfaceObserver {
 public:
  CompositorFrameSinkHolder(Surface* surface,
                            const cc::FrameSinkId& frame_sink_id,
                            cc::SurfaceManager* surface_manager);

  bool HasReleaseCallbackForResource(cc::ResourceId id);
  void SetResourceReleaseCallback(cc::ResourceId id,
                                  const cc::ReleaseCallback& callback);

  CompositorFrameSink* GetCompositorFrameSink() { return frame_sink_.get(); }

  base::WeakPtr<CompositorFrameSinkHolder> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Overridden from cc::mojom::MojoCompositorFrameSinkClient:
  void DidReceiveCompositorFrameAck(
      const cc::ReturnedResourceArray& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;

  // Overridden from cc::ExternalBeginFrameSourceClient:
  void OnNeedsBeginFrames(bool needs_begin_frames) override;
  void OnDidFinishFrame(const cc::BeginFrameAck& ack) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

 private:
  friend class base::RefCounted<CompositorFrameSinkHolder>;

  ~CompositorFrameSinkHolder() override;

  // A collection of callbacks used to release resources.
  using ResourceReleaseCallbackMap = std::map<int, cc::ReleaseCallback>;
  ResourceReleaseCallbackMap release_callbacks_;

  Surface* surface_;
  std::unique_ptr<CompositorFrameSink> frame_sink_;
  std::unique_ptr<cc::ExternalBeginFrameSource> begin_frame_source_;

  base::WeakPtrFactory<CompositorFrameSinkHolder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkHolder);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_COMPOSITOR_FRAME_SINK_HOLDER_H_
