// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_
#define CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_

#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class FakeCompositorFrameSinkSupportClient
    : public viz::CompositorFrameSinkSupportClient {
 public:
  FakeCompositorFrameSinkSupportClient();
  ~FakeCompositorFrameSinkSupportClient() override;

  // CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<ReturnedResource>& resources) override;
  void OnBeginFrame(const BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<ReturnedResource>& resources) override;
  void WillDrawSurface(const viz::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;
  void OnBeginFramePausedChanged(bool paused) override;

  const gfx::Rect& last_damage_rect() const { return last_damage_rect_; }
  const viz::LocalSurfaceId& last_local_surface_id() const {
    return last_local_surface_id_;
  }
  const std::vector<ReturnedResource>& returned_resources() const {
    return returned_resources_;
  }

 private:
  gfx::Rect last_damage_rect_;
  viz::LocalSurfaceId last_local_surface_id_;
  std::vector<ReturnedResource> returned_resources_;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorFrameSinkSupportClient);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_
