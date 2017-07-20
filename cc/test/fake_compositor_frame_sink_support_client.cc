// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_compositor_frame_sink_support_client.h"

#include "cc/surfaces/surface_manager.h"

namespace cc {

FakeCompositorFrameSinkSupportClient::FakeCompositorFrameSinkSupportClient() =
    default;
FakeCompositorFrameSinkSupportClient::~FakeCompositorFrameSinkSupportClient() =
    default;

void FakeCompositorFrameSinkSupportClient::DidReceiveCompositorFrameAck(
    const std::vector<ReturnedResource>& resources) {
  returned_resources_ = resources;
}

void FakeCompositorFrameSinkSupportClient::OnBeginFrame(
    const BeginFrameArgs& args) {}

void FakeCompositorFrameSinkSupportClient::ReclaimResources(
    const std::vector<ReturnedResource>& resources) {
  returned_resources_ = resources;
}

void FakeCompositorFrameSinkSupportClient::WillDrawSurface(
    const viz::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {
  last_local_surface_id_ = local_surface_id;
  last_damage_rect_ = damage_rect;
}

void FakeCompositorFrameSinkSupportClient::OnBeginFramePausedChanged(
    bool paused) {}

};  // namespace cc
