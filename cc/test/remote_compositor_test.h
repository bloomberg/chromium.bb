// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_REMOTE_COMPOSITOR_TEST_H_
#define CC_TEST_REMOTE_COMPOSITOR_TEST_H_

#include "base/macros.h"
#include "cc/blimp/compositor_state_deserializer.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/stub_layer_tree_host_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
class AnimationHost;
class CompositorProtoState;
class FakeLayerTreeHost;
class FakeRemoteCompositorBridge;
class LayerTreeHostRemote;

class RemoteCompositorTest : public testing::Test,
                             public CompositorStateDeserializerClient,
                             public FakeLayerTreeHostClient {
 public:
  RemoteCompositorTest();
  ~RemoteCompositorTest() override;

  void SetUp() override;
  void TearDown() override;

  // CompositorStateDeserializer implementation.
  void DidUpdateLocalState() override;

  // FakeLayerTreeHostClient implementation.
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override;

  bool HasPendingUpdate() const;

  void ProcessCompositorStateUpdate(
      std::unique_ptr<CompositorProtoState> compositor_proto_state);

 protected:
  // Engine setup.
  std::unique_ptr<LayerTreeHostRemote> layer_tree_host_remote_;
  StubLayerTreeHostClient layer_tree_host_client_remote_;
  FakeRemoteCompositorBridge* fake_remote_compositor_bridge_ = nullptr;

  // Client setup.
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_in_process_;
  std::unique_ptr<CompositorStateDeserializer> compositor_state_deserializer_;
  TestTaskGraphRunner task_graph_runner_;

  FakeImageSerializationProcessor image_serialization_processor_;

  bool client_state_dirty_ = false;
};

}  // namespace cc

#endif  // CC_TEST_REMOTE_COMPOSITOR_TEST_H_
