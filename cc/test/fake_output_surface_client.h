// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_
#define CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_

#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"

namespace cc {

class OutputSurface;

class FakeOutputSurfaceClient : public OutputSurfaceClient {
 public:
  FakeOutputSurfaceClient()
      : output_surface_(NULL),
        swap_count_(0),
        deferred_initialize_called_(false),
        did_lose_output_surface_called_(false),
        memory_policy_(0) {}

  explicit FakeOutputSurfaceClient(OutputSurface* output_surface)
      : output_surface_(output_surface),
        swap_count_(0),
        deferred_initialize_called_(false),
        did_lose_output_surface_called_(false),
        memory_policy_(0) {}

  void DeferredInitialize() override;
  void ReleaseGL() override;
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override {}
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override {}
  void DidSwapBuffers() override;
  void DidSwapBuffersComplete() override {}
  void ReclaimResources(const CompositorFrameAck* ack) override {}
  void DidLoseOutputSurface() override;
  void SetExternalDrawConstraints(
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority,
      bool resourceless_software_draw) override {}
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(const base::Closure&) override {}

  int swap_count() { return swap_count_; }

  bool deferred_initialize_called() {
    return deferred_initialize_called_;
  }

  bool did_lose_output_surface_called() {
    return did_lose_output_surface_called_;
  }

  const ManagedMemoryPolicy& memory_policy() const { return memory_policy_; }

 private:
  OutputSurface* output_surface_;
  int swap_count_;
  bool deferred_initialize_called_;
  bool did_lose_output_surface_called_;
  ManagedMemoryPolicy memory_policy_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_
