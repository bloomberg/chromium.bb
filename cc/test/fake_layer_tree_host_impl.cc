// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_impl.h"

#include <stddef.h>

#include "cc/animation/animation_host.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(
    TaskRunnerProvider* task_runner_provider,
    TaskGraphRunner* task_graph_runner)
    : FakeLayerTreeHostImpl(LayerTreeSettings(),
                            task_runner_provider,
                            task_graph_runner) {}

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(
    const LayerTreeSettings& settings,
    TaskRunnerProvider* task_runner_provider,
    TaskGraphRunner* task_graph_runner)
    : FakeLayerTreeHostImpl(settings,
                            task_runner_provider,
                            task_graph_runner,
                            nullptr) {}

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(
    const LayerTreeSettings& settings,
    TaskRunnerProvider* task_runner_provider,
    TaskGraphRunner* task_graph_runner,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner)
    : LayerTreeHostImpl(settings,
                        &client_,
                        task_runner_provider,
                        &stats_instrumentation_,
                        task_graph_runner,
                        AnimationHost::CreateForTesting(ThreadInstance::IMPL),
                        0,
                        std::move(image_worker_task_runner)),
      notify_tile_state_changed_called_(false) {
  // Explicitly clear all debug settings.
  SetDebugState(LayerTreeDebugState());
  SetViewportSize(gfx::Size(100, 100));

  // Start an impl frame so tests have a valid frame_time to work with.
  base::TimeTicks time_ticks = base::TimeTicks::FromInternalValue(1);
  WillBeginImplFrame(
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1, time_ticks));
}

FakeLayerTreeHostImpl::~FakeLayerTreeHostImpl() {
  ReleaseLayerTreeFrameSink();
}

void FakeLayerTreeHostImpl::CreatePendingTree() {
  LayerTreeHostImpl::CreatePendingTree();
  float arbitrary_large_page_scale = 100000.f;
  pending_tree()->PushPageScaleFromMainThread(
      1.f, 1.f / arbitrary_large_page_scale, arbitrary_large_page_scale);
}

void FakeLayerTreeHostImpl::NotifyTileStateChanged(const Tile* tile) {
  LayerTreeHostImpl::NotifyTileStateChanged(tile);
  notify_tile_state_changed_called_ = true;
}

BeginFrameArgs FakeLayerTreeHostImpl::CurrentBeginFrameArgs() const {
  return current_begin_frame_tracker_.DangerousMethodCurrentOrLast();
}

void FakeLayerTreeHostImpl::AdvanceToNextFrame(base::TimeDelta advance_by) {
  BeginFrameArgs next_begin_frame_args = current_begin_frame_tracker_.Current();
  next_begin_frame_args.frame_time += advance_by;
  DidFinishImplFrame();
  WillBeginImplFrame(next_begin_frame_args);
}

void FakeLayerTreeHostImpl::UpdateNumChildrenAndDrawPropertiesForActiveTree() {
  UpdateNumChildrenAndDrawProperties(active_tree());
}

void FakeLayerTreeHostImpl::UpdateNumChildrenAndDrawProperties(
    LayerTreeImpl* layerTree) {
  layerTree->BuildLayerListAndPropertyTreesForTesting();
  layerTree->UpdateDrawProperties();
}

AnimationHost* FakeLayerTreeHostImpl::animation_host() const {
  return static_cast<AnimationHost*>(mutator_host());
}

}  // namespace cc
