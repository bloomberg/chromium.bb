// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/mutator_host.h"

namespace cc {

namespace {

class FakeLayerTree : public LayerTree {
 public:
  FakeLayerTree(MutatorHost* mutator_host, LayerTreeHost* layer_tree_host)
      : LayerTree(mutator_host, layer_tree_host) {}

  void SetNeedsFullTreeSync() override {}
};

}  // namespace

FakeLayerTreeHost::FakeLayerTreeHost(FakeLayerTreeHostClient* client,
                                     LayerTreeHost::InitParams* params,
                                     CompositorMode mode)
    : LayerTreeHost(
          params,
          mode,
          base::MakeUnique<FakeLayerTree>(params->mutator_host, this)),
      client_(client),
      host_impl_(*params->settings,
                 &task_runner_provider_,
                 params->task_graph_runner),
      needs_commit_(false) {
  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner =
      mode == CompositorMode::THREADED ? base::ThreadTaskRunnerHandle::Get()
                                       : nullptr;
  SetTaskRunnerProviderForTesting(TaskRunnerProvider::Create(
      base::ThreadTaskRunnerHandle::Get(), impl_task_runner));
  client_->SetLayerTreeHost(this);
}

std::unique_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner,
    MutatorHost* mutator_host) {
  LayerTreeSettings settings;
  settings.verify_clip_tree_calculations = true;
  return Create(client, task_graph_runner, mutator_host, settings);
}

std::unique_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner,
    MutatorHost* mutator_host,
    const LayerTreeSettings& settings) {
  return Create(client, task_graph_runner, mutator_host, settings,
                CompositorMode::SINGLE_THREADED);
}

std::unique_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    TestTaskGraphRunner* task_graph_runner,
    MutatorHost* mutator_host,
    const LayerTreeSettings& settings,
    CompositorMode mode) {
  LayerTreeHost::InitParams params;
  params.client = client;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner;
  params.mutator_host = mutator_host;
  return base::WrapUnique(new FakeLayerTreeHost(client, &params, mode));
}

FakeLayerTreeHost::~FakeLayerTreeHost() {
  client_->SetLayerTreeHost(NULL);
}

void FakeLayerTreeHost::SetNeedsCommit() { needs_commit_ = true; }

LayerImpl* FakeLayerTreeHost::CommitAndCreateLayerImplTree() {
  TreeSynchronizer::SynchronizeTrees(root_layer(), active_tree());
  active_tree()->SetPropertyTrees(property_trees());
  TreeSynchronizer::PushLayerProperties(root_layer()->GetLayerTree(),
                                        active_tree());
  layer_tree_->mutator_host()->PushPropertiesTo(host_impl_.mutator_host());

  active_tree()->property_trees()->scroll_tree.PushScrollUpdatesFromMainThread(
      property_trees(), active_tree());

  if (layer_tree_->page_scale_layer() &&
      layer_tree_->inner_viewport_scroll_layer()) {
    active_tree()->SetViewportLayersFromIds(
        layer_tree_->overscroll_elasticity_layer()
            ? layer_tree_->overscroll_elasticity_layer()->id()
            : Layer::INVALID_ID,
        layer_tree_->page_scale_layer()->id(),
        layer_tree_->inner_viewport_scroll_layer()->id(),
        layer_tree_->outer_viewport_scroll_layer()
            ? layer_tree_->outer_viewport_scroll_layer()->id()
            : Layer::INVALID_ID);
  }

  active_tree()->UpdatePropertyTreesForBoundsDelta();
  return active_tree()->root_layer_for_testing();
}

LayerImpl* FakeLayerTreeHost::CommitAndCreatePendingTree() {
  TreeSynchronizer::SynchronizeTrees(root_layer(), pending_tree());
  pending_tree()->SetPropertyTrees(property_trees());
  TreeSynchronizer::PushLayerProperties(root_layer()->GetLayerTree(),
                                        pending_tree());
  layer_tree_->mutator_host()->PushPropertiesTo(host_impl_.mutator_host());

  pending_tree()->property_trees()->scroll_tree.PushScrollUpdatesFromMainThread(
      property_trees(), pending_tree());
  return pending_tree()->root_layer_for_testing();
}

}  // namespace cc
