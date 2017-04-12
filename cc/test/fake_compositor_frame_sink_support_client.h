// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_
#define CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_

#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/local_surface_id.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class FakeCompositorFrameSinkSupportClient
    : public CompositorFrameSinkSupportClient {
 public:
  FakeCompositorFrameSinkSupportClient();
  ~FakeCompositorFrameSinkSupportClient() override;

  // CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck(
      const ReturnedResourceArray& resources) override;
  void OnBeginFrame(const BeginFrameArgs& args) override;
  void ReclaimResources(const ReturnedResourceArray& resources) override;
  void WillDrawSurface(const LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

  const gfx::Rect& last_damage_rect() const { return last_damage_rect_; }
  const LocalSurfaceId& last_local_surface_id() const {
    return last_local_surface_id_;
  }
  const ReturnedResourceArray& returned_resources() const {
    return returned_resources_;
  }

 private:
  gfx::Rect last_damage_rect_;
  LocalSurfaceId last_local_surface_id_;
  ReturnedResourceArray returned_resources_;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorFrameSinkSupportClient);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_
