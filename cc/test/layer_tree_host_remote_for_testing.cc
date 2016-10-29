// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_host_remote_for_testing.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/animation_host.h"
#include "cc/blimp/compositor_proto_state.h"
#include "cc/blimp/compositor_state_deserializer.h"
#include "cc/blimp/remote_compositor_bridge.h"
#include "cc/layers/layer.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/remote_client_layer_factory.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_in_process.h"

namespace cc {

class LayerTreeHostRemoteForTesting::RemoteCompositorBridgeImpl
    : public RemoteCompositorBridge {
 public:
  RemoteCompositorBridgeImpl(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner)
      : RemoteCompositorBridge(std::move(compositor_main_task_runner)) {}

  ~RemoteCompositorBridgeImpl() override = default;

  void SetRemoteHost(LayerTreeHostRemoteForTesting* layer_tree_host_remote) {
    DCHECK(layer_tree_host_remote);
    layer_tree_host_remote_ = layer_tree_host_remote;
  }

  // RemoteCompositorBridge implementation.
  void BindToClient(RemoteCompositorBridgeClient* client) override {
    DCHECK(!client_);
    client_ = client;
  }
  void ScheduleMainFrame() override {
    layer_tree_host_remote_->RemoteHostNeedsMainFrame();
  }
  void ProcessCompositorStateUpdate(
      std::unique_ptr<CompositorProtoState> compositor_proto_state) override {
    layer_tree_host_remote_->ProcessRemoteCompositorUpdate(
        std::move(compositor_proto_state));
  }

 private:
  LayerTreeHostRemoteForTesting* layer_tree_host_remote_ = nullptr;
  RemoteCompositorBridgeClient* client_ = nullptr;
};

class LayerTreeHostRemoteForTesting::LayerTreeHostInProcessClient
    : public LayerTreeHostClient {
 public:
  LayerTreeHostInProcessClient(
      LayerTreeHostRemoteForTesting* layer_tree_host_remote)
      : layer_tree_host_remote_(layer_tree_host_remote) {}

  ~LayerTreeHostInProcessClient() override = default;

  void WillBeginMainFrame() override {}
  void BeginMainFrame(const BeginFrameArgs& args) override {
    // Send any scroll/scale updates first.
    layer_tree_host_remote_->ApplyUpdatesFromInProcessHost();
    layer_tree_host_remote_->BeginMainFrame();
  }
  void BeginMainFrameNotExpectedSoon() override {}
  void DidBeginMainFrame() override {}
  void UpdateLayerTreeHost() override {}
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewCompositorFrameSink() override {
    layer_tree_host_remote_->client()->RequestNewCompositorFrameSink();
  }
  void DidInitializeCompositorFrameSink() override {
    layer_tree_host_remote_->client()->DidInitializeCompositorFrameSink();
  }
  void DidFailToInitializeCompositorFrameSink() override {
    layer_tree_host_remote_->client()->DidFailToInitializeCompositorFrameSink();
  }
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {
    layer_tree_host_remote_->client()->DidCommitAndDrawFrame();
  }
  void DidReceiveCompositorFrameAck() override {
    layer_tree_host_remote_->client()->DidReceiveCompositorFrameAck();
  }
  void DidCompletePageScaleAnimation() override {
    NOTREACHED() << "The remote mode doesn't support sending animations";
  }

 private:
  LayerTreeHostRemoteForTesting* layer_tree_host_remote_;
};

// static
std::unique_ptr<RemoteCompositorBridge>
LayerTreeHostRemoteForTesting::CreateRemoteCompositorBridge(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  return base::MakeUnique<RemoteCompositorBridgeImpl>(
      std::move(main_task_runner));
}

// static
std::unique_ptr<LayerTreeHostRemoteForTesting>
LayerTreeHostRemoteForTesting::Create(
    LayerTreeHostClient* client,
    std::unique_ptr<AnimationHost> animation_host,
    LayerTreeSettings const* settings,
    TaskGraphRunner* task_graph_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  std::unique_ptr<FakeImageSerializationProcessor>
      image_serialization_processor =
          base::MakeUnique<FakeImageSerializationProcessor>();

  LayerTreeHostRemote::InitParams params;
  params.client = client;
  params.main_task_runner = main_task_runner;
  params.animation_host = std::move(animation_host);
  params.remote_compositor_bridge =
      CreateRemoteCompositorBridge(main_task_runner);
  params.engine_picture_cache =
      image_serialization_processor->CreateEnginePictureCache();
  params.settings = settings;

  std::unique_ptr<LayerTreeHostRemoteForTesting> layer_tree_host =
      base::WrapUnique(new LayerTreeHostRemoteForTesting(&params));
  layer_tree_host->Initialize(task_graph_runner, main_task_runner,
                              impl_task_runner,
                              std::move(image_serialization_processor));
  return layer_tree_host;
}

LayerTreeHostRemoteForTesting::LayerTreeHostRemoteForTesting(InitParams* params)
    : LayerTreeHostRemote(params),
      layer_tree_host_in_process_client_(
          base::MakeUnique<LayerTreeHostInProcessClient>(this)) {}

LayerTreeHostRemoteForTesting::~LayerTreeHostRemoteForTesting() {
  compositor_state_deserializer_ = nullptr;
  layer_tree_host_in_process_ = nullptr;
}

void LayerTreeHostRemoteForTesting::SetVisible(bool visible) {
  LayerTreeHostRemote::SetVisible(visible);

  // We need to tell the InProcessHost about visibility changes as well.
  layer_tree_host_in_process_->SetVisible(visible);
}

void LayerTreeHostRemoteForTesting::SetCompositorFrameSink(
    std::unique_ptr<CompositorFrameSink> compositor_frame_sink) {
  // Pass through to the InProcessHost since this should be called only in
  // response to a request made by it.
  layer_tree_host_in_process_->SetCompositorFrameSink(
      std::move(compositor_frame_sink));
}

std::unique_ptr<CompositorFrameSink>
LayerTreeHostRemoteForTesting::ReleaseCompositorFrameSink() {
  // Pass through to the InProcessHost since that holds the CompositorFrameSink.
  return layer_tree_host_in_process_->ReleaseCompositorFrameSink();
}

void LayerTreeHostRemoteForTesting::SetNeedsRedrawRect(
    const gfx::Rect& damage_rect) {
  // Draw requests pass through to the InProcessHost.
  layer_tree_host_in_process_->SetNeedsRedrawRect(damage_rect);
}

void LayerTreeHostRemoteForTesting::SetNextCommitForcesRedraw() {
  // Draw requests pass through to the InProcessHost.
  layer_tree_host_in_process_->SetNextCommitForcesRedraw();
}

void LayerTreeHostRemoteForTesting::NotifyInputThrottledUntilCommit() {
  // Pass through because the InProcessHost has the input handler.
  layer_tree_host_in_process_->NotifyInputThrottledUntilCommit();
}

const base::WeakPtr<InputHandler>&
LayerTreeHostRemoteForTesting::GetInputHandler() const {
  return layer_tree_host_in_process_->GetInputHandler();
}

void LayerTreeHostRemoteForTesting::Initialize(
    TaskGraphRunner* task_graph_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    std::unique_ptr<FakeImageSerializationProcessor>
        image_serialization_processor) {
  image_serialization_processor_ = std::move(image_serialization_processor);
  RemoteCompositorBridgeImpl* remote_compositor_bridge_impl =
      static_cast<RemoteCompositorBridgeImpl*>(remote_compositor_bridge());
  remote_compositor_bridge_impl->SetRemoteHost(this);

  layer_tree_host_in_process_ = CreateLayerTreeHostInProcess(
      layer_tree_host_in_process_client_.get(), task_graph_runner,
      GetSettings(), main_task_runner, impl_task_runner);

  compositor_state_deserializer_ =
      base::MakeUnique<CompositorStateDeserializer>(
          layer_tree_host_in_process_.get(),
          image_serialization_processor_->CreateClientPictureCache(),
          base::Bind(&LayerTreeHostRemoteForTesting::LayerDidScroll,
                     base::Unretained(this)),
          this);

  // Override the LayerFactory since a lot of tests rely on the fact that Layers
  // and LayerImpls have matching ids.
  compositor_state_deserializer_->SetLayerFactoryForTesting(
      base::MakeUnique<RemoteClientLayerFactory>());

  // Override the TaskRunnerProvider since tests may rely on accessing the impl
  // task runner using it.
  SetTaskRunnerProviderForTesting(
      TaskRunnerProvider::Create(main_task_runner, impl_task_runner));
}

std::unique_ptr<LayerTreeHostInProcess>
LayerTreeHostRemoteForTesting::CreateLayerTreeHostInProcess(
    LayerTreeHostClient* client,
    TaskGraphRunner* task_graph_runner,
    const LayerTreeSettings& settings,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  LayerTreeHostInProcess::InitParams params;

  params.client = client;
  params.task_graph_runner = task_graph_runner;
  params.settings = &settings;
  params.main_task_runner = main_task_runner;
  params.animation_host = AnimationHost::CreateMainInstance();

  return LayerTreeHostInProcess::CreateThreaded(impl_task_runner, &params);
}

void LayerTreeHostRemoteForTesting::DispatchDrawAndSubmitCallbacks() {
  // Don't dispatch callbacks right after the commit on the remote host. Since
  // tests rely on CompositorFrames being swapped on the CompositorFrameSink,
  // we wait for these callbacks from the LayerTreeHostInProcess.
}

bool LayerTreeHostRemoteForTesting::ShouldRetainClientScroll(
    int engine_layer_id,
    const gfx::ScrollOffset& new_offset) {
  return false;
}

bool LayerTreeHostRemoteForTesting::ShouldRetainClientPageScale(
    float new_page_scale) {
  return false;
}

void LayerTreeHostRemoteForTesting::LayerDidScroll(int engine_layer_id) {
  layers_scrolled_[engine_layer_id] =
      compositor_state_deserializer_->GetLayerForEngineId(engine_layer_id)
          ->scroll_offset();
}

void LayerTreeHostRemoteForTesting::ApplyUpdatesFromInProcessHost() {
  ApplyScrollAndScaleUpdateFromClient(
      layers_scrolled_,
      layer_tree_host_in_process_->GetLayerTree()->page_scale_factor());
  layers_scrolled_.clear();
}

void LayerTreeHostRemoteForTesting::RemoteHostNeedsMainFrame() {
  layer_tree_host_in_process_->SetNeedsAnimate();
}

void LayerTreeHostRemoteForTesting::ProcessRemoteCompositorUpdate(
    std::unique_ptr<CompositorProtoState> compositor_proto_state) {
  DCHECK(layer_tree_host_in_process_->CommitRequested());
  // Deserialize the update from the remote host into client side LTH in
  // process. This bypasses the network layer.
  const proto::LayerTreeHost& layer_tree_host_proto =
      compositor_proto_state->compositor_message->layer_tree_host();
  compositor_state_deserializer_->DeserializeCompositorUpdate(
      layer_tree_host_proto);

  const proto::LayerUpdate& layer_updates =
      compositor_proto_state->compositor_message->layer_tree_host()
          .layer_updates();
  for (int i = 0; i < layer_updates.layers_size(); ++i) {
    int engine_layer_id = layer_updates.layers(i).id();
    Layer* engine_layer = GetLayerTree()->LayerById(engine_layer_id);
    Layer* client_layer =
        compositor_state_deserializer_->GetLayerForEngineId(engine_layer_id);

    // Copy test only layer data that are not serialized into network messages.
    // So in test cases, layers on the client have the same states as their
    // corresponding layers on the engine.
    client_layer->SetForceRenderSurfaceForTesting(
        engine_layer->force_render_surface_for_testing());
  }

  // The only case where the remote host would give a compositor update is if
  // they wanted the main frame to go till the commit pipeline stage. So
  // request one to make sure that the in process main frame also goes till
  // the commit step.
  layer_tree_host_in_process_->SetNeedsCommit();
}

}  // namespace cc
