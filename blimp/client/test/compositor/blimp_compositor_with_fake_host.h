// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_TEST_COMPOSITOR_BLIMP_COMPOSITOR_WITH_FAKE_HOST_H_
#define BLIMP_CLIENT_TEST_COMPOSITOR_BLIMP_COMPOSITOR_WITH_FAKE_HOST_H_

#include "base/macros.h"
#include "blimp/client/core/compositor/blimp_compositor.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/test_task_graph_runner.h"

namespace cc {
namespace proto {
class CompositorMessage;
}
}

namespace blimp {
namespace client {

class BlimpCompositorWithFakeHost : public BlimpCompositor {
 public:
  static std::unique_ptr<BlimpCompositorWithFakeHost> Create(
      BlimpCompositorDependencies* compositor_dependencies,
      BlimpCompositorClient* client);
  ~BlimpCompositorWithFakeHost() override;

  std::unique_ptr<cc::proto::CompositorMessage> CreateFakeUpdate(
      scoped_refptr<cc::Layer> root_layer);
  cc::FakeLayerTreeHost* host() { return fake_host_; }

 protected:
  BlimpCompositorWithFakeHost(
      BlimpCompositorDependencies* compositor_dependencies,
      BlimpCompositorClient* client);

  std::unique_ptr<cc::LayerTreeHostInProcess> CreateLayerTreeHost() override;

 private:
  cc::FakeLayerTreeHostClient fake_client_;
  cc::TestTaskGraphRunner task_graph_runner_;
  cc::FakeLayerTreeHost* fake_host_ = nullptr;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_TEST_COMPOSITOR_BLIMP_COMPOSITOR_WITH_FAKE_HOST_H_
