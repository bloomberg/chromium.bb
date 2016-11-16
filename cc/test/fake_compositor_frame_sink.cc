// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_compositor_frame_sink.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/resources/returned_resource.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

FakeCompositorFrameSink::FakeCompositorFrameSink(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider)
    : CompositorFrameSink(std::move(context_provider),
                          std::move(worker_context_provider),
                          nullptr,
                          nullptr),
      weak_ptr_factory_(this) {
  gpu_memory_buffer_manager_ =
      context_provider_ ? &test_gpu_memory_buffer_manager_ : nullptr;
  shared_bitmap_manager_ =
      context_provider_ ? nullptr : &test_shared_bitmap_manager_;
}

FakeCompositorFrameSink::~FakeCompositorFrameSink() = default;

void FakeCompositorFrameSink::DetachFromClient() {
  ReturnResourcesHeldByParent();
  CompositorFrameSink::DetachFromClient();
}

void FakeCompositorFrameSink::SubmitCompositorFrame(CompositorFrame frame) {
  ReturnResourcesHeldByParent();

  last_sent_frame_.reset(new CompositorFrame(std::move(frame)));
  ++num_sent_frames_;

  if (last_sent_frame_->delegated_frame_data) {
    auto* frame_data = last_sent_frame_->delegated_frame_data.get();
    last_swap_rect_ = frame_data->render_pass_list.back()->damage_rect;
    last_swap_rect_valid_ = true;
  } else {
    last_swap_rect_ = gfx::Rect();
    last_swap_rect_valid_ = false;
  }

  if (last_sent_frame_->delegated_frame_data || !context_provider()) {
    if (last_sent_frame_->delegated_frame_data) {
      auto* frame_data = last_sent_frame_->delegated_frame_data.get();
      resources_held_by_parent_.insert(resources_held_by_parent_.end(),
                                       frame_data->resource_list.begin(),
                                       frame_data->resource_list.end());
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&FakeCompositorFrameSink::DidReceiveCompositorFrameAck,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FakeCompositorFrameSink::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAck();
}

void FakeCompositorFrameSink::ReturnResourcesHeldByParent() {
  // Check |delegated_frame_data| because we shouldn't reclaim resources
  // for the Display which does not swap delegated frames.
  if (last_sent_frame_ && last_sent_frame_->delegated_frame_data) {
    // Return the last frame's resources immediately.
    ReturnedResourceArray resources;
    for (const auto& resource : resources_held_by_parent_)
      resources.push_back(resource.ToReturnedResource());
    resources_held_by_parent_.clear();
    client_->ReclaimResources(resources);
  }
}

}  // namespace cc
