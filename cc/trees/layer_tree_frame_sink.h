// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_FRAME_SINK_H_
#define CC_TREES_LAYER_TREE_FRAME_SINK_H_

#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "cc/cc_export.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/gfx/color_space.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {
class CompositorFrame;
class LocalSurfaceId;
class SharedBitmapManager;
struct BeginFrameAck;
}  // namespace viz

namespace cc {
class LayerTreeFrameSinkClient;
class LayerTreeHostImpl;

// An interface for submitting CompositorFrames to a display compositor
// which will compose frames from multiple clients to show on screen to the
// user.
// If a context_provider() is present, frames should be submitted with
// OpenGL resources (created with the context_provider()). If not, then
// SharedBitmap resources should be used.
class CC_EXPORT LayerTreeFrameSink : public viz::ContextLostObserver {
 public:
  struct Capabilities {
    Capabilities() = default;

    // True if we must always swap, even if there is no damage to the frame.
    // Needed for both the browser compositor as well as layout tests.
    // TODO(ericrk): This should be test-only for layout tests, but tab
    // capture has issues capturing offscreen tabs whithout this. We should
    // remove this dependency. crbug.com/680196
    bool must_always_swap = false;

    // True if sync points for resources are needed when swapping delegated
    // frames.
    bool delegated_sync_points_required = true;
  };

  // Constructor for GL-based and/or software resources.
  //
  // |compositor_task_runner| is used to post worker context lost callback and
  // must belong to the same thread where all calls to or from client are made.
  // Optional and won't be used unless |worker_context_provider| is present.
  //
  // |gpu_memory_buffer_manager| and |shared_bitmap_manager| must outlive the
  // LayerTreeFrameSink. |shared_bitmap_manager| is optional (won't be used) if
  // |context_provider| is present. |gpu_memory_buffer_manager| is optional
  // (won't be used) unless |context_provider| is present.
  LayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      viz::SharedBitmapManager* shared_bitmap_manager);

  ~LayerTreeFrameSink() override;

  base::WeakPtr<LayerTreeFrameSink> GetWeakPtr();

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point to when DetachFromClient() is called the output surface will
  // only be used on the compositor thread.
  // The caller should call DetachFromClient() on the same thread before
  // destroying the LayerTreeFrameSink, even if this fails. And BindToClient
  // should not be called twice for a given LayerTreeFrameSink.
  virtual bool BindToClient(LayerTreeFrameSinkClient* client);

  // Must be called from the thread where BindToClient was called if
  // BindToClient succeeded, after which the LayerTreeFrameSink may be
  // destroyed from any thread. This is a place where thread-specific data for
  // the object can be uninitialized.
  virtual void DetachFromClient();

  bool HasClient() { return !!client_; }

  const Capabilities& capabilities() const { return capabilities_; }

  // The viz::ContextProviders may be null if frames should be submitted with
  // software SharedBitmap resources.
  viz::ContextProvider* context_provider() const {
    return context_provider_.get();
  }
  viz::RasterContextProvider* worker_context_provider() const {
    return worker_context_provider_.get();
  }
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_;
  }
  viz::SharedBitmapManager* shared_bitmap_manager() const {
    return shared_bitmap_manager_;
  }

  // Generate hit test region list based on LayerTreeHostImpl, the data will be
  // submitted with compositor frame.
  virtual void UpdateHitTestData(const LayerTreeHostImpl* host_impl) {}

  // If supported, this sets the viz::LocalSurfaceId the LayerTreeFrameSink will
  // use to submit a CompositorFrame.
  virtual void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) {}

  // Support for a pull-model where draws are requested by the implementation of
  // LayerTreeFrameSink. This is called by the compositor to notify that there's
  // new content.
  virtual void Invalidate() {}

  // For successful swaps, the implementation must call
  // DidReceiveCompositorFrameAck() asynchronously when the frame has been
  // processed in order to unthrottle the next frame.
  virtual void SubmitCompositorFrame(viz::CompositorFrame frame) = 0;

  // Signals that a BeginFrame issued by the viz::BeginFrameSource provided to
  // the client did not lead to a CompositorFrame submission.
  virtual void DidNotProduceFrame(const viz::BeginFrameAck& ack) = 0;

  // Associates a SharedBitmapId with a shared buffer handle.
  virtual void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                                       const viz::SharedBitmapId& id) = 0;

  // Disassociates a SharedBitmapId previously passed to
  // DidAllocateSharedBitmap.
  virtual void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) = 0;

 protected:
  class ContextLostForwarder;

  // viz::ContextLostObserver:
  void OnContextLost() override;

  LayerTreeFrameSinkClient* client_ = nullptr;

  struct LayerTreeFrameSink::Capabilities capabilities_;
  scoped_refptr<viz::ContextProvider> context_provider_;
  scoped_refptr<viz::RasterContextProvider> worker_context_provider_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  viz::SharedBitmapManager* shared_bitmap_manager_;

  std::unique_ptr<ContextLostForwarder> worker_context_lost_forwarder_;

 private:
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<LayerTreeFrameSink> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(LayerTreeFrameSink);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_FRAME_SINK_H_
