// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COMPOSITOR_FRAME_SINK_H_
#define CC_OUTPUT_COMPOSITOR_FRAME_SINK_H_

#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/vulkan_context_provider.h"
#include "cc/resources/returned_resource.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/color_space.h"

namespace ui {
class LatencyInfo;
}

namespace gfx {
class ColorSpace;
class Rect;
class Size;
class Transform;
}

namespace cc {

class CompositorFrame;
struct ManagedMemoryPolicy;
class CompositorFrameSinkClient;

// Represents the output surface for a compositor. The compositor owns
// and manages its destruction. Its lifetime is:
//   1. Created on the main thread by the LayerTreeHost through its client.
//   2. Passed to the compositor thread and bound to a client via BindToClient.
//      From here on, it will only be used on the compositor thread.
//   3. If the 3D context is lost, then the compositor will delete the output
//      surface (on the compositor thread) and go back to step 1.
class CC_EXPORT CompositorFrameSink
    : public base::trace_event::MemoryDumpProvider {
 public:
  struct Capabilities {
    Capabilities() = default;

    // Whether ForceReclaimResources can be called to reclaim all resources
    // from the CompositorFrameSink.
    bool can_force_reclaim_resources = false;
    // True if sync points for resources are needed when swapping delegated
    // frames.
    bool delegated_sync_points_required = true;
  };

  // Constructor for GL-based and/or software compositing.
  CompositorFrameSink(scoped_refptr<ContextProvider> context_provider,
                      scoped_refptr<ContextProvider> worker_context_provider);

  // Constructor for Vulkan-based compositing.
  explicit CompositorFrameSink(
      scoped_refptr<VulkanContextProvider> vulkan_context_provider);

  ~CompositorFrameSink() override;

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point to when DetachFromClient() is called the output surface will
  // only be used on the compositor thread.
  // The caller should call DetachFromClient() on the same thread before
  // destroying the CompositorFrameSink, even if this fails. And BindToClient
  // should not be called twice for a given CompositorFrameSink.
  virtual bool BindToClient(CompositorFrameSinkClient* client);

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be uninitialized.
  virtual void DetachFromClient();

  bool HasClient() { return !!client_; }

  const Capabilities& capabilities() const { return capabilities_; }

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  ContextProvider* context_provider() const { return context_provider_.get(); }
  ContextProvider* worker_context_provider() const {
    return worker_context_provider_.get();
  }
  VulkanContextProvider* vulkan_context_provider() const {
    return vulkan_context_provider_.get();
  }

  // If supported, this causes a ReclaimResources for all resources that are
  // currently in use.
  virtual void ForceReclaimResources() {}

  // Support for a pull-model where draws are requested by the output surface.
  //
  // CompositorFrameSink::Invalidate is called by the compositor to notify that
  // there's new content.
  virtual void Invalidate() {}

  // For successful swaps, the implementation must call DidSwapBuffersComplete()
  // (via OnSwapBuffersComplete()) eventually.
  virtual void SwapBuffers(CompositorFrame frame) = 0;
  virtual void OnSwapBuffersComplete();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 protected:
  // This is used by both display and delegating implementations.
  void PostSwapBuffersComplete();

  // Bound to the ContextProvider to hear about when it is lost and inform the
  // |client_|.
  void DidLoseCompositorFrameSink();

  CompositorFrameSinkClient* client_ = nullptr;

  struct CompositorFrameSink::Capabilities capabilities_;
  scoped_refptr<ContextProvider> context_provider_;
  scoped_refptr<ContextProvider> worker_context_provider_;
  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;
  base::ThreadChecker client_thread_checker_;

 private:
  void DetachFromClientInternal();

  base::WeakPtrFactory<CompositorFrameSink> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSink);
};

}  // namespace cc

#endif  // CC_OUTPUT_COMPOSITOR_FRAME_SINK_H_
