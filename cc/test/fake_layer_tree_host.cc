// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/test_task_graph_runner.h"

namespace cc {
FakeLayerTreeHost::FakeLayerTreeHost(FakeLayerTreeHostClient* client,
                                     LayerTreeHost::InitParams* params,
                                     CompositorMode mode)
    : LayerTreeHost(params, mode),
      client_(client),
      host_impl_(*params->settings,
                 &task_runner_provider_,
                 &manager_,
                 params->task_graph_runner),
      needs_commit_(false),
      renderer_capabilities_set(false) {
  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner =
      mode == CompositorMode::THREADED ? base::ThreadTaskRunnerHandle::Get()
                                       : nullptr;
  SetTaskRunnerProviderForTesting(TaskRunnerProvider::Create(
      base::ThreadTaskRunnerHandle::Get(), impl_task_runner));
  client_->SetLayerTreeHost(this);
}

std::unique_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner) {
  return Create(client, task_graph_runner, LayerTreeSettings());
}

std::unique_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner,
    const LayerTreeSettings& settings) {
  return Create(client, task_graph_runner, settings,
                CompositorMode::SINGLE_THREADED);
}

std::unique_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner,
    const LayerTreeSettings& settings,
    CompositorMode mode) {
  LayerTreeHost::InitParams params;
  params.client = client;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner;
  params.animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  return base::WrapUnique(new FakeLayerTreeHost(client, &params, mode));
}

std::unique_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner,
    const LayerTreeSettings& settings,
    CompositorMode mode,
    ImageSerializationProcessor* image_serialization_processor) {
  LayerTreeHost::InitParams params;
  params.client = client;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner;
  params.image_serialization_processor = image_serialization_processor;
  params.animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  return base::WrapUnique(new FakeLayerTreeHost(client, &params, mode));
}

FakeLayerTreeHost::~FakeLayerTreeHost() {
  client_->SetLayerTreeHost(NULL);
}

const RendererCapabilities& FakeLayerTreeHost::GetRendererCapabilities() const {
  if (renderer_capabilities_set)
    return renderer_capabilities;
  return LayerTreeHost::GetRendererCapabilities();
}

void FakeLayerTreeHost::SetNeedsCommit() { needs_commit_ = true; }

LayerImpl* FakeLayerTreeHost::CommitAndCreateLayerImplTree() {
  TreeSynchronizer::SynchronizeTrees(root_layer(), active_tree());
  active_tree()->SetPropertyTrees(property_trees());
  TreeSynchronizer::PushLayerProperties(root_layer()->layer_tree_host(),
                                        active_tree());
  animation_host()->PushPropertiesTo(host_impl_.animation_host());

  active_tree()->UpdatePropertyTreeScrollOffset(property_trees());

  if (page_scale_layer() && inner_viewport_scroll_layer()) {
    active_tree()->SetViewportLayersFromIds(
        overscroll_elasticity_layer() ? overscroll_elasticity_layer()->id()
                                      : Layer::INVALID_ID,
        page_scale_layer()->id(), inner_viewport_scroll_layer()->id(),
        outer_viewport_scroll_layer() ? outer_viewport_scroll_layer()->id()
                                      : Layer::INVALID_ID);
  }

  active_tree()->UpdatePropertyTreesForBoundsDelta();
  return active_tree()->root_layer_for_testing();
}

LayerImpl* FakeLayerTreeHost::CommitAndCreatePendingTree() {
  TreeSynchronizer::SynchronizeTrees(root_layer(), pending_tree());
  pending_tree()->SetPropertyTrees(property_trees());
  TreeSynchronizer::PushLayerProperties(root_layer()->layer_tree_host(),
                                        pending_tree());
  animation_host()->PushPropertiesTo(host_impl_.animation_host());

  pending_tree()->UpdatePropertyTreeScrollOffset(property_trees());
  return pending_tree()->root_layer_for_testing();
}

}  // namespace cc
