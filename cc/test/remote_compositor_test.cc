// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/remote_compositor_test.h"

#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/blimp/compositor_proto_state.h"
#include "cc/blimp/layer_tree_host_remote.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_remote_compositor_bridge.h"

namespace cc {
namespace {

class ProxyForCommitRequest : public FakeProxy {
 public:
  bool CommitRequested() const override { return true; }
};

class RemoteCompositorBridgeForTest : public FakeRemoteCompositorBridge {
 public:
  using ProtoFrameCallback = base::Callback<void(
      std::unique_ptr<CompositorProtoState> compositor_proto_state)>;

  RemoteCompositorBridgeForTest(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      ProtoFrameCallback proto_frame_callback)
      : FakeRemoteCompositorBridge(main_task_runner),
        proto_frame_callback_(proto_frame_callback) {}

  ~RemoteCompositorBridgeForTest() override = default;

  void ProcessCompositorStateUpdate(
      std::unique_ptr<CompositorProtoState> compositor_proto_state) override {
    proto_frame_callback_.Run(std::move(compositor_proto_state));
  }

 private:
  ProtoFrameCallback proto_frame_callback_;
};

}  // namespace

RemoteCompositorTest::RemoteCompositorTest() = default;

RemoteCompositorTest::~RemoteCompositorTest() = default;

void RemoteCompositorTest::SetUp() {
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
      base::ThreadTaskRunnerHandle::Get();

  animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

  // Engine side setup.
  LayerTreeHostRemote::InitParams params;
  params.client = &layer_tree_host_client_remote_;
  params.main_task_runner = main_task_runner;
  params.mutator_host = animation_host_.get();
  std::unique_ptr<RemoteCompositorBridgeForTest> bridge_for_test =
      base::MakeUnique<RemoteCompositorBridgeForTest>(
          main_task_runner,
          base::Bind(&RemoteCompositorTest::ProcessCompositorStateUpdate,
                     base::Unretained(this)));
  fake_remote_compositor_bridge_ = bridge_for_test.get();
  params.remote_compositor_bridge = std::move(bridge_for_test);
  params.engine_picture_cache =
      image_serialization_processor_.CreateEnginePictureCache();
  LayerTreeSettings settings;
  params.settings = &settings;

  layer_tree_host_remote_ = base::MakeUnique<LayerTreeHostRemote>(&params);

  // Client side setup.
  layer_tree_host_in_process_ = FakeLayerTreeHost::Create(
      this, &task_graph_runner_, animation_host_.get(), settings,
      CompositorMode::THREADED);
  layer_tree_host_in_process_->InitializeForTesting(
      TaskRunnerProvider::Create(base::ThreadTaskRunnerHandle::Get(),
                                 base::ThreadTaskRunnerHandle::Get()),
      base::MakeUnique<ProxyForCommitRequest>());
  std::unique_ptr<ClientPictureCache> client_picture_cache =
      image_serialization_processor_.CreateClientPictureCache();
  compositor_state_deserializer_ =
      base::MakeUnique<CompositorStateDeserializer>(
          layer_tree_host_in_process_.get(), std::move(client_picture_cache),
          this);
}

void RemoteCompositorTest::TearDown() {
  fake_remote_compositor_bridge_ = nullptr;
  layer_tree_host_remote_ = nullptr;
  compositor_state_deserializer_ = nullptr;
  layer_tree_host_in_process_ = nullptr;
  animation_host_ = nullptr;
}

// CompositorStateDeserializer implementation.
void RemoteCompositorTest::DidUpdateLocalState() {
  client_state_dirty_ = true;
}

void RemoteCompositorTest::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  compositor_state_deserializer_->ApplyViewportDeltas(
      inner_delta, outer_delta, elastic_overscroll_delta, page_scale,
      top_controls_delta);
}

bool RemoteCompositorTest::HasPendingUpdate() const {
  DCHECK(fake_remote_compositor_bridge_);
  return fake_remote_compositor_bridge_->has_pending_update();
}

void RemoteCompositorTest::ProcessCompositorStateUpdate(
    std::unique_ptr<CompositorProtoState> compositor_proto_state) {
  compositor_state_deserializer_->DeserializeCompositorUpdate(
      compositor_proto_state->compositor_message->layer_tree_host());
}

}  // namespace cc
