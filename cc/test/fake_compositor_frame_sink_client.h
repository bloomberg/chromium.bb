// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_CLIENT_H_
#define CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_CLIENT_H_

#include "cc/output/compositor_frame_sink_client.h"
#include "cc/output/managed_memory_policy.h"

namespace cc {

class CompositorFrameSink;

class FakeCompositorFrameSinkClient : public CompositorFrameSinkClient {
 public:
  FakeCompositorFrameSinkClient()
      : swap_count_(0),
        did_lose_compositor_frame_sink_called_(false),
        memory_policy_(0) {}

  void SetBeginFrameSource(BeginFrameSource* source) override {}
  void DidSwapBuffersComplete() override;
  void ReclaimResources(const ReturnedResourceArray& resources) override {}
  void DidLoseCompositorFrameSink() override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override {}
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(const base::Closure&) override {}
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override {}

  int swap_count() { return swap_count_; }

  bool did_lose_compositor_frame_sink_called() {
    return did_lose_compositor_frame_sink_called_;
  }

  const ManagedMemoryPolicy& memory_policy() const { return memory_policy_; }

 private:
  int swap_count_;
  bool did_lose_compositor_frame_sink_called_;
  ManagedMemoryPolicy memory_policy_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_CLIENT_H_
