// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/test/compositor/blimp_compositor_with_fake_host.h"

#include "cc/animation/animation_host.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/test/fake_proxy.h"

namespace blimp {
namespace client {

// static
std::unique_ptr<BlimpCompositorWithFakeHost>
BlimpCompositorWithFakeHost::Create(
    BlimpCompositorDependencies* compositor_dependencies,
    BlimpCompositorClient* client) {
  std::unique_ptr<BlimpCompositorWithFakeHost> compositor = base::WrapUnique(
      new BlimpCompositorWithFakeHost(compositor_dependencies, client));
  compositor->Initialize();
  return compositor;
}

BlimpCompositorWithFakeHost::BlimpCompositorWithFakeHost(
    BlimpCompositorDependencies* compositor_dependencies,
    BlimpCompositorClient* client)
    : BlimpCompositor(compositor_dependencies, client) {}

BlimpCompositorWithFakeHost::~BlimpCompositorWithFakeHost() = default;

std::unique_ptr<cc::proto::CompositorMessage>
BlimpCompositorWithFakeHost::CreateFakeUpdate(
    scoped_refptr<cc::Layer> root_layer) {
  std::unique_ptr<cc::proto::CompositorMessage> message =
      base::MakeUnique<cc::proto::CompositorMessage>();
  message->set_client_state_update_ack(false);
  cc::LayerTree* layer_tree = host()->GetLayerTree();
  layer_tree->SetRootLayer(root_layer);
  cc::proto::LayerTree* layer_tree_proto =
      message->mutable_layer_tree_host()->mutable_layer_tree();
  layer_tree->ToProtobuf(layer_tree_proto);
  return message;
}

std::unique_ptr<cc::LayerTreeHostInProcess>
BlimpCompositorWithFakeHost::CreateLayerTreeHost() {
  DCHECK(animation_host());

  cc::LayerTreeSettings settings;
  std::unique_ptr<cc::FakeLayerTreeHost> host = cc::FakeLayerTreeHost::Create(
      &fake_client_, &task_graph_runner_, animation_host(), settings,
      cc::CompositorMode::THREADED);
  host->InitializeForTesting(
      cc::TaskRunnerProvider::Create(base::ThreadTaskRunnerHandle::Get(),
                                     base::ThreadTaskRunnerHandle::Get()),
      base::MakeUnique<cc::FakeProxy>());
  fake_host_ = host.get();
  return std::move(host);
}

}  // namespace client
}  // namespace blimp
