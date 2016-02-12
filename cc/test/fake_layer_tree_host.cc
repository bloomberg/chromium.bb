// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host.h"

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

scoped_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner) {
  LayerTreeSettings settings;
  settings.verify_property_trees = true;
  settings.use_compositor_animation_timelines = true;
  return Create(client, task_graph_runner, settings);
}

scoped_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner,
    const LayerTreeSettings& settings) {
  return Create(client, task_graph_runner, settings,
                CompositorMode::SINGLE_THREADED);
}

scoped_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner,
    const LayerTreeSettings& settings,
    CompositorMode mode) {
  LayerTreeHost::InitParams params;
  params.client = client;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner;
  return make_scoped_ptr(new FakeLayerTreeHost(client, &params, mode));
}

scoped_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
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
  return make_scoped_ptr(new FakeLayerTreeHost(client, &params, mode));
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
  scoped_ptr<LayerImpl> old_root_layer_impl = active_tree()->DetachLayerTree();

  scoped_ptr<LayerImpl> layer_impl = TreeSynchronizer::SynchronizeTrees(
      root_layer(), std::move(old_root_layer_impl), active_tree());
  active_tree()->SetPropertyTrees(*property_trees());
  TreeSynchronizer::PushProperties(root_layer(), layer_impl.get());

  active_tree()->SetRootLayer(std::move(layer_impl));

  if (page_scale_layer() && inner_viewport_scroll_layer()) {
    active_tree()->SetViewportLayersFromIds(
        overscroll_elasticity_layer() ? overscroll_elasticity_layer()->id()
                                      : Layer::INVALID_ID,
        page_scale_layer()->id(), inner_viewport_scroll_layer()->id(),
        outer_viewport_scroll_layer() ? outer_viewport_scroll_layer()->id()
                                      : Layer::INVALID_ID);
  }

  active_tree()->UpdatePropertyTreesForBoundsDelta();
  return active_tree()->root_layer();
}

}  // namespace cc
