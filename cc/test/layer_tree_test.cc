// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_test.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/timing_function.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/proto/compositor_message_to_impl.pb.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/remote_channel_impl_for_test.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/test/threaded_channel_for_test.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/threaded_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

void CreateVirtualViewportLayers(Layer* root_layer,
                                 scoped_refptr<Layer> outer_scroll_layer,
                                 const gfx::Size& inner_bounds,
                                 const gfx::Size& outer_bounds,
                                 LayerTreeHost* host) {
  scoped_refptr<Layer> inner_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
  scoped_refptr<Layer> outer_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> page_scale_layer = Layer::Create();

  root_layer->AddChild(inner_viewport_container_layer);
  inner_viewport_container_layer->AddChild(page_scale_layer);
  page_scale_layer->AddChild(inner_viewport_scroll_layer);
  inner_viewport_scroll_layer->AddChild(outer_viewport_container_layer);
  outer_viewport_container_layer->AddChild(outer_scroll_layer);

  inner_viewport_scroll_layer->SetScrollClipLayerId(
      inner_viewport_container_layer->id());
  outer_scroll_layer->SetScrollClipLayerId(
      outer_viewport_container_layer->id());

  inner_viewport_container_layer->SetBounds(inner_bounds);
  inner_viewport_scroll_layer->SetBounds(outer_bounds);
  outer_viewport_container_layer->SetBounds(outer_bounds);

  inner_viewport_scroll_layer->SetIsContainerForFixedPositionLayers(true);
  outer_scroll_layer->SetIsContainerForFixedPositionLayers(true);
  host->RegisterViewportLayers(NULL, page_scale_layer,
                               inner_viewport_scroll_layer, outer_scroll_layer);
}

void CreateVirtualViewportLayers(Layer* root_layer,
                                 const gfx::Size& inner_bounds,
                                 const gfx::Size& outer_bounds,
                                 const gfx::Size& scroll_bounds,
                                 LayerTreeHost* host) {
  scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();

  outer_viewport_scroll_layer->SetBounds(scroll_bounds);
  outer_viewport_scroll_layer->SetIsDrawable(true);
  CreateVirtualViewportLayers(root_layer, outer_viewport_scroll_layer,
                              inner_bounds, outer_bounds, host);
}

// Creates a SingleThreadProxy that notifies the supplied |test_hooks| of
// various actions.
class SingleThreadProxyForTest : public SingleThreadProxy {
 public:
  static std::unique_ptr<Proxy> Create(
      TestHooks* test_hooks,
      LayerTreeHost* host,
      LayerTreeHostSingleThreadClient* client,
      TaskRunnerProvider* task_runner_provider) {
    return base::WrapUnique(new SingleThreadProxyForTest(
        test_hooks, host, client, task_runner_provider));
  }

  ~SingleThreadProxyForTest() override {}

 private:
  SingleThreadProxyForTest(TestHooks* test_hooks,
                           LayerTreeHost* host,
                           LayerTreeHostSingleThreadClient* client,
                           TaskRunnerProvider* task_runner_provider)
      : SingleThreadProxy(host, client, task_runner_provider),
        test_hooks_(test_hooks) {}

  void ScheduledActionSendBeginMainFrame(const BeginFrameArgs& args) override {
    test_hooks_->ScheduledActionWillSendBeginMainFrame();
    SingleThreadProxy::ScheduledActionSendBeginMainFrame(args);
    test_hooks_->ScheduledActionSendBeginMainFrame();
  }

  DrawResult ScheduledActionDrawAndSwapIfPossible() override {
    DrawResult result =
        SingleThreadProxy::ScheduledActionDrawAndSwapIfPossible();
    test_hooks_->ScheduledActionDrawAndSwapIfPossible();
    return result;
  }

  void ScheduledActionCommit() override {
    SingleThreadProxy::ScheduledActionCommit();
    test_hooks_->ScheduledActionCommit();
  }

  void ScheduledActionBeginOutputSurfaceCreation() override {
    SingleThreadProxy::ScheduledActionBeginOutputSurfaceCreation();
    test_hooks_->ScheduledActionBeginOutputSurfaceCreation();
  }

  void ScheduledActionPrepareTiles() override {
    SingleThreadProxy::ScheduledActionPrepareTiles();
    test_hooks_->ScheduledActionPrepareTiles();
  }

  void ScheduledActionInvalidateOutputSurface() override {
    SingleThreadProxy::ScheduledActionInvalidateOutputSurface();
    test_hooks_->ScheduledActionInvalidateOutputSurface();
  }

  void SendBeginMainFrameNotExpectedSoon() override {
    SingleThreadProxy::SendBeginMainFrameNotExpectedSoon();
    test_hooks_->SendBeginMainFrameNotExpectedSoon();
  }

  TestHooks* test_hooks_;
};

// Adapts LayerTreeHostImpl for test. Runs real code, then invokes test hooks.
class LayerTreeHostImplForTesting : public LayerTreeHostImpl {
 public:
  static std::unique_ptr<LayerTreeHostImplForTesting> Create(
      TestHooks* test_hooks,
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* host_impl_client,
      TaskRunnerProvider* task_runner_provider,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* stats_instrumentation) {
    return base::WrapUnique(new LayerTreeHostImplForTesting(
        test_hooks, settings, host_impl_client, task_runner_provider,
        shared_bitmap_manager, gpu_memory_buffer_manager, task_graph_runner,
        stats_instrumentation));
  }

 protected:
  LayerTreeHostImplForTesting(
      TestHooks* test_hooks,
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* host_impl_client,
      TaskRunnerProvider* task_runner_provider,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* stats_instrumentation)
      : LayerTreeHostImpl(settings,
                          host_impl_client,
                          task_runner_provider,
                          stats_instrumentation,
                          shared_bitmap_manager,
                          gpu_memory_buffer_manager,
                          task_graph_runner,
                          0),
        test_hooks_(test_hooks),
        block_notify_ready_to_activate_for_testing_(false),
        notify_ready_to_activate_was_blocked_(false) {}

  void CreateResourceAndRasterBufferProvider(
      std::unique_ptr<RasterBufferProvider>* raster_buffer_provider,
      std::unique_ptr<ResourcePool>* resource_pool) override {
    test_hooks_->CreateResourceAndRasterBufferProvider(
        this, raster_buffer_provider, resource_pool);
  }

  void WillBeginImplFrame(const BeginFrameArgs& args) override {
    LayerTreeHostImpl::WillBeginImplFrame(args);
    test_hooks_->WillBeginImplFrameOnThread(this, args);
  }

  void DidFinishImplFrame() override {
    LayerTreeHostImpl::DidFinishImplFrame();
    test_hooks_->DidFinishImplFrameOnThread(this);
  }

  void BeginMainFrameAborted(CommitEarlyOutReason reason) override {
    LayerTreeHostImpl::BeginMainFrameAborted(reason);
    test_hooks_->BeginMainFrameAbortedOnThread(this, reason);
  }

  void BeginCommit() override {
    LayerTreeHostImpl::BeginCommit();
    test_hooks_->BeginCommitOnThread(this);
  }

  void CommitComplete() override {
    test_hooks_->WillCommitCompleteOnThread(this);
    LayerTreeHostImpl::CommitComplete();
    test_hooks_->CommitCompleteOnThread(this);
  }

  bool PrepareTiles() override {
    test_hooks_->WillPrepareTiles(this);
    return LayerTreeHostImpl::PrepareTiles();
  }

  DrawResult PrepareToDraw(FrameData* frame) override {
    DrawResult draw_result = LayerTreeHostImpl::PrepareToDraw(frame);
    return test_hooks_->PrepareToDrawOnThread(this, frame, draw_result);
  }

  void DrawLayers(FrameData* frame) override {
    LayerTreeHostImpl::DrawLayers(frame);
    test_hooks_->DrawLayersOnThread(this);
  }

  bool SwapBuffers(const LayerTreeHostImpl::FrameData& frame) override {
    bool result = LayerTreeHostImpl::SwapBuffers(frame);
    test_hooks_->SwapBuffersOnThread(this, result);
    return result;
  }

  void DidSwapBuffersComplete() override {
    LayerTreeHostImpl::DidSwapBuffersComplete();
    test_hooks_->SwapBuffersCompleteOnThread(this);
  }

  void ReclaimResources(const CompositorFrameAck* ack) override {
    LayerTreeHostImpl::ReclaimResources(ack);
  }

  void NotifyReadyToActivate() override {
    if (block_notify_ready_to_activate_for_testing_) {
      notify_ready_to_activate_was_blocked_ = true;
    } else {
      LayerTreeHostImpl::NotifyReadyToActivate();
      test_hooks_->NotifyReadyToActivateOnThread(this);
    }
  }

  void NotifyReadyToDraw() override {
    LayerTreeHostImpl::NotifyReadyToDraw();
    test_hooks_->NotifyReadyToDrawOnThread(this);
  }

  void NotifyAllTileTasksCompleted() override {
    LayerTreeHostImpl::NotifyAllTileTasksCompleted();
    test_hooks_->NotifyAllTileTasksCompleted(this);
  }

  void BlockNotifyReadyToActivateForTesting(bool block) override {
    CHECK(task_runner_provider()->ImplThreadTaskRunner())
        << "Not supported for single-threaded mode.";
    block_notify_ready_to_activate_for_testing_ = block;
    if (!block && notify_ready_to_activate_was_blocked_) {
      NotifyReadyToActivate();
      notify_ready_to_activate_was_blocked_ = false;
    }
  }

  void ActivateSyncTree() override {
    test_hooks_->WillActivateTreeOnThread(this);
    LayerTreeHostImpl::ActivateSyncTree();
    DCHECK(!pending_tree());
    test_hooks_->DidActivateTreeOnThread(this);
  }

  bool InitializeRenderer(OutputSurface* output_surface) override {
    bool success = LayerTreeHostImpl::InitializeRenderer(output_surface);
    test_hooks_->InitializedRendererOnThread(this, success);
    return success;
  }

  void SetVisible(bool visible) override {
    LayerTreeHostImpl::SetVisible(visible);
    test_hooks_->DidSetVisibleOnImplTree(this, visible);
  }

  bool AnimateLayers(base::TimeTicks monotonic_time) override {
    test_hooks_->WillAnimateLayers(this, monotonic_time);
    bool result = LayerTreeHostImpl::AnimateLayers(monotonic_time);
    test_hooks_->AnimateLayers(this, monotonic_time);
    return result;
  }

  void UpdateAnimationState(bool start_ready_animations) override {
    LayerTreeHostImpl::UpdateAnimationState(start_ready_animations);
    bool has_unfinished_animation = false;
    for (const auto& it :
         animation_host()->active_element_animations_for_testing()) {
      if (it.second->HasActiveAnimation()) {
        has_unfinished_animation = true;
        break;
      }
    }
    test_hooks_->UpdateAnimationState(this, has_unfinished_animation);
  }

  void NotifyTileStateChanged(const Tile* tile) override {
    LayerTreeHostImpl::NotifyTileStateChanged(tile);
    test_hooks_->NotifyTileStateChangedOnThread(this, tile);
  }

 private:
  TestHooks* test_hooks_;
  bool block_notify_ready_to_activate_for_testing_;
  bool notify_ready_to_activate_was_blocked_;
};

// Implementation of LayerTreeHost callback interface.
class LayerTreeHostClientForTesting : public LayerTreeHostClient,
                                      public LayerTreeHostSingleThreadClient {
 public:
  static std::unique_ptr<LayerTreeHostClientForTesting> Create(
      TestHooks* test_hooks) {
    return base::WrapUnique(new LayerTreeHostClientForTesting(test_hooks));
  }
  ~LayerTreeHostClientForTesting() override {}

  void WillBeginMainFrame() override { test_hooks_->WillBeginMainFrame(); }

  void DidBeginMainFrame() override { test_hooks_->DidBeginMainFrame(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    test_hooks_->BeginMainFrame(args);
  }

  void UpdateLayerTreeHost() override {
    test_hooks_->UpdateLayerTreeHost();
  }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {
    test_hooks_->ApplyViewportDeltas(inner_delta, outer_delta,
                                     elastic_overscroll_delta, page_scale,
                                     top_controls_delta);
  }

  void RequestNewOutputSurface() override {
    test_hooks_->RequestNewOutputSurface();
  }

  void DidInitializeOutputSurface() override {
    test_hooks_->DidInitializeOutputSurface();
  }

  void DidFailToInitializeOutputSurface() override {
    test_hooks_->DidFailToInitializeOutputSurface();
    RequestNewOutputSurface();
  }

  void WillCommit() override { test_hooks_->WillCommit(); }

  void DidCommit() override { test_hooks_->DidCommit(); }

  void DidCommitAndDrawFrame() override {
    test_hooks_->DidCommitAndDrawFrame();
  }

  void DidCompleteSwapBuffers() override {
    test_hooks_->DidCompleteSwapBuffers();
  }

  void DidPostSwapBuffers() override {}
  void DidAbortSwapBuffers() override {}
  void RequestScheduleComposite() override { test_hooks_->ScheduleComposite(); }
  void DidCompletePageScaleAnimation() override {}
  void BeginMainFrameNotExpectedSoon() override {}

 private:
  explicit LayerTreeHostClientForTesting(TestHooks* test_hooks)
      : test_hooks_(test_hooks) {}

  TestHooks* test_hooks_;
};

// Adapts LayerTreeHost for test. Injects LayerTreeHostImplForTesting.
class LayerTreeHostForTesting : public LayerTreeHost {
 public:
  static std::unique_ptr<LayerTreeHostForTesting> Create(
      TestHooks* test_hooks,
      CompositorMode mode,
      LayerTreeHostClientForTesting* client,
      RemoteProtoChannel* remote_proto_channel,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      TaskGraphRunner* task_graph_runner,
      const LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
    LayerTreeHost::InitParams params;
    params.client = client;
    params.shared_bitmap_manager = shared_bitmap_manager;
    params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
    params.task_graph_runner = task_graph_runner;
    params.settings = &settings;
    std::unique_ptr<LayerTreeHostForTesting> layer_tree_host(
        new LayerTreeHostForTesting(test_hooks, &params, mode));
    std::unique_ptr<TaskRunnerProvider> task_runner_provider =
        TaskRunnerProvider::Create(main_task_runner, impl_task_runner);
    std::unique_ptr<Proxy> proxy;
    switch (mode) {
      case CompositorMode::SINGLE_THREADED:
        proxy = SingleThreadProxyForTest::Create(test_hooks,
                                                 layer_tree_host.get(), client,
                                                 task_runner_provider.get());
        break;
      case CompositorMode::THREADED:
        DCHECK(impl_task_runner.get());
        proxy = ProxyMainForTest::CreateThreaded(
            test_hooks, layer_tree_host.get(), task_runner_provider.get());
        break;
      case CompositorMode::REMOTE:
        DCHECK(!external_begin_frame_source);
        // The Remote LayerTreeHost on the client has the impl task runner.
        if (task_runner_provider->HasImplThread()) {
          proxy = RemoteChannelImplForTest::Create(
              test_hooks, layer_tree_host.get(), remote_proto_channel,
              task_runner_provider.get());
        } else {
          proxy = ProxyMainForTest::CreateRemote(
              test_hooks, remote_proto_channel, layer_tree_host.get(),
              task_runner_provider.get());

          // The LayerTreeHost on the server will never have an output surface.
          // Set output_surface_lost_ to false by default.
          layer_tree_host->SetOutputSurfaceLostForTesting(false);
        }
        break;
    }
    layer_tree_host->InitializeForTesting(
        std::move(task_runner_provider), std::move(proxy),
        std::move(external_begin_frame_source));
    return layer_tree_host;
  }

  std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* host_impl_client) override {
    return LayerTreeHostImplForTesting::Create(
        test_hooks_, settings(), host_impl_client, task_runner_provider(),
        shared_bitmap_manager(), gpu_memory_buffer_manager(),
        task_graph_runner(), rendering_stats_instrumentation());
  }

  void SetNeedsCommit() override {
    if (!test_started_)
      return;
    LayerTreeHost::SetNeedsCommit();
  }

  void SetNeedsUpdateLayers() override {
    if (!test_started_)
      return;
    LayerTreeHost::SetNeedsUpdateLayers();
  }

  void set_test_started(bool started) { test_started_ = started; }

 private:
  LayerTreeHostForTesting(TestHooks* test_hooks,
                          LayerTreeHost::InitParams* params,
                          CompositorMode mode)
      : LayerTreeHost(params, mode),
        test_hooks_(test_hooks),
        test_started_(false) {}

  TestHooks* test_hooks_;
  bool test_started_;
};

LayerTreeTest::LayerTreeTest()
    : output_surface_(nullptr),
      external_begin_frame_source_(nullptr),
      remote_proto_channel_bridge_(this),
      beginning_(false),
      end_when_begin_returns_(false),
      timed_out_(false),
      scheduled_(false),
      started_(false),
      ended_(false),
      delegating_renderer_(false),
      timeout_seconds_(0),
      weak_factory_(this) {
  main_thread_weak_ptr_ = weak_factory_.GetWeakPtr();

  // Tests should timeout quickly unless --cc-layer-tree-test-no-timeout was
  // specified (for running in a debugger).
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kCCLayerTreeTestNoTimeout))
    timeout_seconds_ = 5;
}

LayerTreeTest::~LayerTreeTest() {}

Proxy* LayerTreeTest::remote_client_proxy() const {
  DCHECK(IsRemoteTest());
  return remote_client_layer_tree_host_
             ? remote_client_layer_tree_host_->proxy()
             : nullptr;
}

bool LayerTreeTest::IsRemoteTest() const {
  return mode_ == CompositorMode::REMOTE;
}

gfx::Vector2dF LayerTreeTest::ScrollDelta(LayerImpl* layer_impl) {
  gfx::ScrollOffset delta =
      layer_impl->layer_tree_impl()
          ->property_trees()
          ->scroll_tree.GetScrollOffsetDeltaForTesting(layer_impl->id());
  return gfx::Vector2dF(delta.x(), delta.y());
}

void LayerTreeTest::EndTest() {
  if (ended_)
    return;
  ended_ = true;

  // For the case where we EndTest during BeginTest(), set a flag to indicate
  // that the test should end the second BeginTest regains control.
  if (beginning_) {
    end_when_begin_returns_ = true;
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeTest::RealEndTest, main_thread_weak_ptr_));
  }
}

void LayerTreeTest::EndTestAfterDelayMs(int delay_milliseconds) {
  main_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::EndTest, main_thread_weak_ptr_),
      base::TimeDelta::FromMilliseconds(delay_milliseconds));
}

void LayerTreeTest::PostAddAnimationToMainThreadPlayer(
    AnimationPlayer* player_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchAddAnimationToPlayer,
                 main_thread_weak_ptr_,
                 base::Unretained(player_to_receive_animation), 0.000004));
}

void LayerTreeTest::PostAddInstantAnimationToMainThreadPlayer(
    AnimationPlayer* player_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchAddAnimationToPlayer,
                 main_thread_weak_ptr_,
                 base::Unretained(player_to_receive_animation), 0.0));
}

void LayerTreeTest::PostAddLongAnimationToMainThreadPlayer(
    AnimationPlayer* player_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchAddAnimationToPlayer,
                 main_thread_weak_ptr_,
                 base::Unretained(player_to_receive_animation), 1.0));
}

void LayerTreeTest::PostSetDeferCommitsToMainThread(bool defer_commits) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchSetDeferCommits,
                 main_thread_weak_ptr_, defer_commits));
}

void LayerTreeTest::PostSetNeedsCommitToMainThread() {
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&LayerTreeTest::DispatchSetNeedsCommit,
                                         main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsUpdateLayersToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchSetNeedsUpdateLayers,
                 main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsRedrawToMainThread() {
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&LayerTreeTest::DispatchSetNeedsRedraw,
                                         main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsRedrawRectToMainThread(
    const gfx::Rect& damage_rect) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchSetNeedsRedrawRect,
                 main_thread_weak_ptr_,
                 damage_rect));
}

void LayerTreeTest::PostSetVisibleToMainThread(bool visible) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &LayerTreeTest::DispatchSetVisible, main_thread_weak_ptr_, visible));
}

void LayerTreeTest::PostSetNextCommitForcesRedrawToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchSetNextCommitForcesRedraw,
                 main_thread_weak_ptr_));
}

void LayerTreeTest::PostCompositeImmediatelyToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchCompositeImmediately,
                 main_thread_weak_ptr_));
}

void LayerTreeTest::PostNextCommitWaitsForActivationToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchNextCommitWaitsForActivation,
                 main_thread_weak_ptr_));
}

void LayerTreeTest::SetOutputSurfaceOnLayerTreeHost(
    std::unique_ptr<OutputSurface> output_surface) {
  if (IsRemoteTest()) {
    DCHECK(remote_client_layer_tree_host_);
    remote_client_layer_tree_host_->SetOutputSurface(std::move(output_surface));
  } else {
    layer_tree_host_->SetOutputSurface(std::move(output_surface));
  }
}

std::unique_ptr<OutputSurface>
LayerTreeTest::ReleaseOutputSurfaceOnLayerTreeHost() {
  if (IsRemoteTest()) {
    DCHECK(remote_client_layer_tree_host_);
    return remote_client_layer_tree_host_->ReleaseOutputSurface();
  }
  return layer_tree_host_->ReleaseOutputSurface();
}

void LayerTreeTest::SetVisibleOnLayerTreeHost(bool visible) {
  layer_tree_host_->SetVisible(visible);

  if (IsRemoteTest()) {
    DCHECK(remote_client_layer_tree_host_);
    remote_client_layer_tree_host_->SetVisible(visible);
  }
}

void LayerTreeTest::WillBeginTest() {
  SetVisibleOnLayerTreeHost(true);
}

void LayerTreeTest::DoBeginTest() {
  client_ = LayerTreeHostClientForTesting::Create(this);

  std::unique_ptr<FakeExternalBeginFrameSource> external_begin_frame_source;
  if (settings_.use_external_begin_frame_source) {
    DCHECK(!IsRemoteTest());
    external_begin_frame_source.reset(new FakeExternalBeginFrameSource(
        settings_.renderer_settings.refresh_rate));
    external_begin_frame_source_ = external_begin_frame_source.get();
  }

  DCHECK(!impl_thread_ || impl_thread_->task_runner().get());

  if (IsRemoteTest()) {
    DCHECK(impl_thread_);
    layer_tree_host_ = LayerTreeHostForTesting::Create(
        this, mode_, client_.get(), &remote_proto_channel_bridge_.channel_main,
        nullptr, nullptr, task_graph_runner_.get(), settings_,
        base::ThreadTaskRunnerHandle::Get(), nullptr,
        std::move(external_begin_frame_source));
    DCHECK(remote_proto_channel_bridge_.channel_main.HasReceiver());
  } else {
    layer_tree_host_ = LayerTreeHostForTesting::Create(
        this, mode_, client_.get(), nullptr, shared_bitmap_manager_.get(),
        gpu_memory_buffer_manager_.get(), task_graph_runner_.get(), settings_,
        base::ThreadTaskRunnerHandle::Get(),
        impl_thread_ ? impl_thread_->task_runner() : NULL,
        std::move(external_begin_frame_source));
  }

  ASSERT_TRUE(layer_tree_host_);

  started_ = true;
  beginning_ = true;
  SetupTree();
  WillBeginTest();
  BeginTest();
  beginning_ = false;
  if (end_when_begin_returns_)
    RealEndTest();

  // Allow commits to happen once BeginTest() has had a chance to post tasks
  // so that those tasks will happen before the first commit.
  if (layer_tree_host_) {
    static_cast<LayerTreeHostForTesting*>(layer_tree_host_.get())
        ->set_test_started(true);
  }
}

void LayerTreeTest::SetupTree() {
  if (!layer_tree_host_->root_layer()) {
    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(1, 1));
    layer_tree_host_->SetRootLayer(root_layer);
  }

  gfx::Size root_bounds = layer_tree_host_->root_layer()->bounds();
  gfx::Size device_root_bounds = gfx::ScaleToCeiledSize(
      root_bounds, layer_tree_host_->device_scale_factor());
  layer_tree_host_->SetViewportSize(device_root_bounds);
  layer_tree_host_->root_layer()->SetIsDrawable(true);
}

void LayerTreeTest::Timeout() {
  timed_out_ = true;
  EndTest();
}

void LayerTreeTest::RealEndTest() {
  // TODO(mithro): Make this method only end when not inside an impl frame.
  bool main_frame_will_happen;
  if (IsRemoteTest()) {
    main_frame_will_happen =
        remote_client_layer_tree_host_
            ? remote_client_proxy()->MainFrameWillHappenForTesting()
            : false;
  } else {
    main_frame_will_happen =
        layer_tree_host_ ? proxy()->MainFrameWillHappenForTesting() : false;
  }

  if (main_frame_will_happen && !timed_out_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeTest::RealEndTest, main_thread_weak_ptr_));
    return;
  }

  base::MessageLoop::current()->QuitWhenIdle();
}

void LayerTreeTest::DispatchAddAnimationToPlayer(
    AnimationPlayer* player_to_receive_animation,
    double animation_duration) {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());

  if (player_to_receive_animation) {
    AddOpacityTransitionToPlayer(player_to_receive_animation,
                                 animation_duration, 0, 0.5, true);
  }
}

void LayerTreeTest::DispatchSetDeferCommits(bool defer_commits) {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->SetDeferCommits(defer_commits);
}

void LayerTreeTest::DispatchSetNeedsCommit() {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->SetNeedsCommit();
}

void LayerTreeTest::DispatchSetNeedsUpdateLayers() {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->SetNeedsUpdateLayers();
}

void LayerTreeTest::DispatchSetNeedsRedraw() {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->SetNeedsRedraw();
}

void LayerTreeTest::DispatchSetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->SetNeedsRedrawRect(damage_rect);
}

void LayerTreeTest::DispatchSetVisible(bool visible) {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());
  if (layer_tree_host_)
    SetVisibleOnLayerTreeHost(visible);
}

void LayerTreeTest::DispatchSetNextCommitForcesRedraw() {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->SetNextCommitForcesRedraw();
}

void LayerTreeTest::DispatchCompositeImmediately() {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());
  if (layer_tree_host_)
    layer_tree_host_->Composite(base::TimeTicks::Now());
}

void LayerTreeTest::DispatchNextCommitWaitsForActivation() {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNextCommitWaitsForActivation();
}

void LayerTreeTest::RunTest(CompositorMode mode, bool delegating_renderer) {
  mode_ = mode;
  if (mode_ == CompositorMode::THREADED || mode_ == CompositorMode::REMOTE) {
    impl_thread_.reset(new base::Thread("Compositor"));
    ASSERT_TRUE(impl_thread_->Start());
  }

  main_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  shared_bitmap_manager_.reset(new TestSharedBitmapManager);
  gpu_memory_buffer_manager_.reset(new TestGpuMemoryBufferManager);
  task_graph_runner_.reset(new TestTaskGraphRunner);

  delegating_renderer_ = delegating_renderer;

  // Spend less time waiting for BeginFrame because the output is
  // mocked out.
  settings_.renderer_settings.refresh_rate = 200.0;
  settings_.background_animation_rate = 200.0;
  InitializeSettings(&settings_);

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DoBeginTest, base::Unretained(this)));

  if (timeout_seconds_) {
    timeout_.Reset(base::Bind(&LayerTreeTest::Timeout, base::Unretained(this)));
    main_task_runner_->PostDelayedTask(
        FROM_HERE,
        timeout_.callback(),
        base::TimeDelta::FromSeconds(timeout_seconds_));
  }

  base::MessageLoop::current()->Run();
  DestroyLayerTreeHost();

  timeout_.Cancel();

  ASSERT_FALSE(layer_tree_host_.get());
  client_ = nullptr;
  if (timed_out_) {
    FAIL() << "Test timed out";
    return;
  }
  AfterTest();
}

void LayerTreeTest::RequestNewOutputSurface() {
  SetOutputSurfaceOnLayerTreeHost(CreateOutputSurface());
}

std::unique_ptr<OutputSurface> LayerTreeTest::CreateOutputSurface() {
  std::unique_ptr<FakeOutputSurface> output_surface = CreateFakeOutputSurface();
  DCHECK_EQ(delegating_renderer_,
            output_surface->capabilities().delegated_rendering);
  output_surface_ = output_surface.get();

  if (settings_.use_external_begin_frame_source &&
      settings_.wait_for_beginframe_interval) {
    DCHECK(external_begin_frame_source_);
  }
  return std::move(output_surface);
}

std::unique_ptr<FakeOutputSurface> LayerTreeTest::CreateFakeOutputSurface() {
  if (delegating_renderer_)
    return FakeOutputSurface::CreateDelegating3d();
  else
    return FakeOutputSurface::Create3d();
}

TestWebGraphicsContext3D* LayerTreeTest::TestContext() {
  return static_cast<TestContextProvider*>(output_surface_->context_provider())
      ->TestContext3d();
}

void LayerTreeTest::DestroyLayerTreeHost() {
  if (layer_tree_host_ && layer_tree_host_->root_layer())
    layer_tree_host_->root_layer()->SetLayerTreeHost(NULL);
  layer_tree_host_ = nullptr;

  DCHECK(!remote_proto_channel_bridge_.channel_main.HasReceiver());

  // Destroying the LayerTreeHost should destroy the remote client
  // LayerTreeHost.
  DCHECK(!remote_client_layer_tree_host_);
}

void LayerTreeTest::DestroyRemoteClientHost() {
  DCHECK(IsRemoteTest());
  DCHECK(remote_client_layer_tree_host_);

  remote_client_layer_tree_host_ = nullptr;
  DCHECK(!remote_proto_channel_bridge_.channel_impl.HasReceiver());
}

void LayerTreeTest::CreateRemoteClientHost(
    const proto::CompositorMessageToImpl& proto) {
  DCHECK(IsRemoteTest());
  DCHECK(!remote_client_layer_tree_host_);
  DCHECK(impl_thread_);
  DCHECK(proto.message_type() ==
         proto::CompositorMessageToImpl::INITIALIZE_IMPL);

  proto::InitializeImpl initialize_proto = proto.initialize_impl_message();
  LayerTreeSettings settings;
  settings.FromProtobuf(initialize_proto.layer_tree_settings());
  settings.abort_commit_before_output_surface_creation = false;
  remote_client_layer_tree_host_ = LayerTreeHostForTesting::Create(
      this, mode_, client_.get(), &remote_proto_channel_bridge_.channel_impl,
      nullptr, nullptr, task_graph_runner_.get(), settings,
      base::ThreadTaskRunnerHandle::Get(), impl_thread_->task_runner(),
      nullptr);

  DCHECK(remote_proto_channel_bridge_.channel_impl.HasReceiver());
  DCHECK(task_runner_provider()->HasImplThread());
}

TaskGraphRunner* LayerTreeTest::task_graph_runner() const {
  return task_graph_runner_.get();
}

TaskRunnerProvider* LayerTreeTest::task_runner_provider() const {
  // All LayerTreeTests can use the task runner provider to access the impl
  // thread. In the remote mode, the impl thread of the compositor lives on
  // the client, so return the task runner provider owned by the remote client
  // LayerTreeHost.
  if (IsRemoteTest()) {
    return remote_client_layer_tree_host_
               ? remote_client_layer_tree_host_->task_runner_provider()
               : nullptr;
  }
  return layer_tree_host_ ? layer_tree_host_->task_runner_provider() : nullptr;
}

LayerTreeHost* LayerTreeTest::layer_tree_host() {
  // We check for a null task_runner_provider here as we sometimes ask for the
  // layer tree host when the task_runner_provider does not exist, often for
  // checking settings after a test has completed. For example,
  // LTHPixelResourceTest::RunPixelResourceTest. See elsewhere in this file for
  // other examples.
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread() ||
         task_runner_provider()->IsMainThreadBlocked());
  return layer_tree_host_.get();
}

LayerTreeHost* LayerTreeTest::remote_client_layer_tree_host() {
  DCHECK(!task_runner_provider() || task_runner_provider()->IsMainThread() ||
         task_runner_provider()->IsMainThreadBlocked());
  DCHECK(IsRemoteTest());
  return remote_client_layer_tree_host_.get();
}

ProxyMainForTest* LayerTreeTest::GetProxyMainForTest() const {
  DCHECK(HasImplThread());
  return static_cast<ProxyMainForTest*>(proxy());
}

ProxyImplForTest* LayerTreeTest::GetProxyImplForTest() const {
  DCHECK(HasImplThread());

  if (IsRemoteTest()) {
    return GetRemoteChannelImplForTest()->proxy_impl_for_test();
  }

  ProxyImplForTest* proxy_impl_for_test =
      GetThreadedChannelForTest()->proxy_impl_for_test();

  // We check for null ProxyImpl since ProxyImpl exists in the ThreadedChannel
  // only after it is initialized.
  DCHECK(proxy_impl_for_test);
  return proxy_impl_for_test;
}

ThreadedChannelForTest* LayerTreeTest::GetThreadedChannelForTest() const {
  DCHECK(mode_ == CompositorMode::THREADED);

  return GetProxyMainForTest()->threaded_channel_for_test();
}

RemoteChannelImplForTest* LayerTreeTest::GetRemoteChannelImplForTest() const {
  DCHECK(IsRemoteTest());
  DCHECK(remote_client_layer_tree_host_);

  return static_cast<RemoteChannelImplForTest*>(
      remote_client_layer_tree_host_->proxy());
}

}  // namespace cc
