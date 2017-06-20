// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_synchronous_compositor_android.h"

#include <utility>

#include "cc/output/compositor_frame.h"

namespace content {

TestSynchronousCompositor::TestSynchronousCompositor(int process_id,
                                                     int routing_id)
    : client_(NULL), process_id_(process_id), routing_id_(routing_id) {}

TestSynchronousCompositor::~TestSynchronousCompositor() {
  SetClient(NULL);
}

void TestSynchronousCompositor::SetClient(SynchronousCompositorClient* client) {
  if (client_)
    client_->DidDestroyCompositor(this, process_id_, routing_id_);
  client_ = client;
  if (client_)
    client_->DidInitializeCompositor(this, process_id_, routing_id_);
}

SynchronousCompositor::Frame TestSynchronousCompositor::DemandDrawHw(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  return std::move(hardware_frame_);
}

scoped_refptr<SynchronousCompositor::FrameFuture>
TestSynchronousCompositor::DemandDrawHwAsync(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  return nullptr;
}

void TestSynchronousCompositor::ReturnResources(
    uint32_t layer_tree_frame_sink_id,
    const cc::ReturnedResourceArray& resources) {
  ReturnedResources returned_resources;
  returned_resources.layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  returned_resources.resources = resources;
  frame_ack_array_.push_back(returned_resources);
}

void TestSynchronousCompositor::SwapReturnedResources(FrameAckArray* array) {
  DCHECK(array);
  frame_ack_array_.swap(*array);
}

bool TestSynchronousCompositor::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(canvas);
  return true;
}

void TestSynchronousCompositor::SetHardwareFrame(
    uint32_t layer_tree_frame_sink_id,
    std::unique_ptr<cc::CompositorFrame> frame) {
  hardware_frame_.layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  hardware_frame_.frame = std::move(frame);
}

TestSynchronousCompositor::ReturnedResources::ReturnedResources()
    : layer_tree_frame_sink_id(0u) {}

TestSynchronousCompositor::ReturnedResources::ReturnedResources(
    const ReturnedResources& other) = default;

TestSynchronousCompositor::ReturnedResources::~ReturnedResources() {}

}  // namespace content
