// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_TEST_H_
#define CC_TEST_LAYER_TREE_TEST_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "cc/animation/animation_delegate.h"
#include "cc/layers/layer_settings.h"
#include "cc/test/proxy_impl_for_test.h"
#include "cc/test/proxy_main_for_test.h"
#include "cc/test/remote_proto_channel_bridge.h"
#include "cc/test/test_hooks.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
class AnimationPlayer;
class FakeExternalBeginFrameSource;
class FakeLayerTreeHostClient;
class FakeOutputSurface;
class LayerImpl;
class LayerTreeHost;
class LayerTreeHostForTesting;
class LayerTreeHostClient;
class LayerTreeHostImpl;
class RemoteChannelImplForTest;
class TestContextProvider;
class TestGpuMemoryBufferManager;
class TestTaskGraphRunner;
class TestWebGraphicsContext3D;
class ThreadedChannelForTest;

// Creates the virtual viewport layer hierarchy under the given root_layer.
// Convenient overload of the method below that creates a scrolling layer as
// the outer viewport scroll layer.
void CreateVirtualViewportLayers(Layer* root_layer,
                                 const gfx::Size& inner_bounds,
                                 const gfx::Size& outer_bounds,
                                 const gfx::Size& scroll_bounds,
                                 LayerTreeHost* host,
                                 const LayerSettings& layer_settings);

// Creates the virtual viewport layer hierarchy under the given root_layer.
// Uses the given scroll layer as the content "outer viewport scroll layer".
void CreateVirtualViewportLayers(Layer* root_layer,
                                 scoped_refptr<Layer> outer_scroll_layer,
                                 const gfx::Size& outer_bounds,
                                 const gfx::Size& scroll_bounds,
                                 LayerTreeHost* host,
                                 const LayerSettings& layer_settings);

class BeginTask;
class LayerTreeHostClientForTesting;
class TimeoutTask;

// The LayerTreeTests runs with the main loop running. It instantiates a single
// LayerTreeHostForTesting and associated LayerTreeHostImplForTesting and
// LayerTreeHostClientForTesting.
//
// BeginTest() is called once the main message loop is running and the layer
// tree host is initialized.
//
// Key stages of the drawing loop, e.g. drawing or commiting, redirect to
// LayerTreeTest methods of similar names. To track the commit process, override
// these functions.
//
// The test continues until someone calls EndTest. EndTest can be called on any
// thread, but be aware that ending the test is an asynchronous process.
class LayerTreeTest : public testing::Test, public TestHooks {
 public:
  ~LayerTreeTest() override;

  virtual void EndTest();
  void EndTestAfterDelayMs(int delay_milliseconds);

  void PostAddAnimationToMainThread(Layer* layer_to_receive_animation);
  void PostAddInstantAnimationToMainThread(Layer* layer_to_receive_animation);
  void PostAddLongAnimationToMainThread(Layer* layer_to_receive_animation);
  void PostAddAnimationToMainThreadPlayer(
      AnimationPlayer* player_to_receive_animation);
  void PostAddInstantAnimationToMainThreadPlayer(
      AnimationPlayer* player_to_receive_animation);
  void PostAddLongAnimationToMainThreadPlayer(
      AnimationPlayer* player_to_receive_animation);
  void PostSetDeferCommitsToMainThread(bool defer_commits);
  void PostSetNeedsCommitToMainThread();
  void PostSetNeedsUpdateLayersToMainThread();
  void PostSetNeedsRedrawToMainThread();
  void PostSetNeedsRedrawRectToMainThread(const gfx::Rect& damage_rect);
  void PostSetVisibleToMainThread(bool visible);
  void PostSetNextCommitForcesRedrawToMainThread();
  void PostCompositeImmediatelyToMainThread();
  void PostNextCommitWaitsForActivationToMainThread();

  void DoBeginTest();
  void Timeout();

  bool verify_property_trees() const { return verify_property_trees_; }
  void set_verify_property_trees(bool verify_property_trees) {
    verify_property_trees_ = verify_property_trees;
  }

  const LayerSettings& layer_settings() { return layer_settings_; }

 protected:
  LayerTreeTest();

  virtual void InitializeSettings(LayerTreeSettings* settings) {}
  virtual void InitializeLayerSettings(LayerSettings* layer_settings) {}

  void RealEndTest();

  virtual void DispatchAddAnimation(Layer* layer_to_receive_animation,
                                    double animation_duration);
  virtual void DispatchAddAnimationToPlayer(
      AnimationPlayer* player_to_receive_animation,
      double animation_duration);
  void DispatchSetDeferCommits(bool defer_commits);
  void DispatchSetNeedsCommit();
  void DispatchSetNeedsUpdateLayers();
  void DispatchSetNeedsRedraw();
  void DispatchSetNeedsRedrawRect(const gfx::Rect& damage_rect);
  void DispatchSetVisible(bool visible);
  void DispatchSetNextCommitForcesRedraw();
  void DispatchDidAddAnimation();
  void DispatchCompositeImmediately();
  void DispatchNextCommitWaitsForActivation();

  void SetOutputSurfaceOnLayerTreeHost(
      scoped_ptr<OutputSurface> output_surface);
  scoped_ptr<OutputSurface> ReleaseOutputSurfaceOnLayerTreeHost();
  void SetVisibleOnLayerTreeHost(bool visible);

  virtual void AfterTest() = 0;
  virtual void WillBeginTest();
  virtual void BeginTest() = 0;
  virtual void SetupTree();

  virtual void RunTest(CompositorMode mode, bool delegating_renderer);

  bool HasImplThread() const { return !!impl_thread_; }
  base::SingleThreadTaskRunner* ImplThreadTaskRunner() {
    DCHECK(task_runner_provider());
    base::SingleThreadTaskRunner* impl_thread_task_runner =
        task_runner_provider()->ImplThreadTaskRunner();
    return impl_thread_task_runner ? impl_thread_task_runner
                                   : main_task_runner_.get();
  }
  base::SingleThreadTaskRunner* MainThreadTaskRunner() {
    return main_task_runner_.get();
  }
  Proxy* proxy() const {
    return layer_tree_host_ ? layer_tree_host_->proxy() : NULL;
  }
  Proxy* remote_client_proxy() const;
  TaskRunnerProvider* task_runner_provider() const;
  TaskGraphRunner* task_graph_runner() const;
  bool TestEnded() const { return ended_; }

  LayerTreeHost* layer_tree_host();
  bool delegating_renderer() const { return delegating_renderer_; }
  FakeOutputSurface* output_surface() { return output_surface_; }
  int LastCommittedSourceFrameNumber(LayerTreeHostImpl* impl) const;

  // Use these only for tests in threaded or remote mode.
  ProxyMainForTest* GetProxyMainForTest() const;
  ProxyImplForTest* GetProxyImplForTest() const;

  // Use this only for tests in threaded mode.
  ThreadedChannelForTest* GetThreadedChannelForTest() const;

  // Use this only for tests in remote mode.
  RemoteChannelImplForTest* GetRemoteChannelImplForTest() const;

  void DestroyLayerTreeHost();
  void DestroyRemoteClientHost() override;

  void CreateRemoteClientHost(
      const proto::CompositorMessageToImpl& proto) override;

  // By default, output surface recreation is synchronous.
  void RequestNewOutputSurface() override;
  // Override this for pixel tests, where you need a real output surface.
  virtual scoped_ptr<OutputSurface> CreateOutputSurface();
  // Override this for unit tests, which should not produce pixel output.
  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface();

  TestWebGraphicsContext3D* TestContext();

  TestGpuMemoryBufferManager* GetTestGpuMemoryBufferManager() {
    return gpu_memory_buffer_manager_.get();
  }

  bool IsRemoteTest() const;

 private:
  LayerTreeSettings settings_;
  LayerSettings layer_settings_;

  CompositorMode mode_;

  scoped_ptr<LayerTreeHostClientForTesting> client_;
  scoped_ptr<LayerTreeHost> layer_tree_host_;

  // The LayerTreeHost created by the cc embedder on the client in remote mode.
  scoped_ptr<LayerTreeHostForTesting> remote_client_layer_tree_host_;

  FakeOutputSurface* output_surface_;
  FakeExternalBeginFrameSource* external_begin_frame_source_;
  RemoteProtoChannelBridge remote_proto_channel_bridge_;

  bool beginning_;
  bool end_when_begin_returns_;
  bool timed_out_;
  bool scheduled_;
  bool started_;
  bool ended_;
  bool delegating_renderer_;
  bool verify_property_trees_;

  int timeout_seconds_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_ptr<base::Thread> impl_thread_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<TestGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  scoped_ptr<TestTaskGraphRunner> task_graph_runner_;
  base::CancelableClosure timeout_;
  scoped_refptr<TestContextProvider> compositor_contexts_;
  base::WeakPtr<LayerTreeTest> main_thread_weak_ptr_;
  base::WeakPtrFactory<LayerTreeTest> weak_factory_;
};

}  // namespace cc

#define SINGLE_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunSingleThread_DirectRenderer) {   \
    RunTest(CompositorMode::SINGLE_THREADED, false);            \
  }                                                             \
  class SingleThreadDirectImplNeedsSemicolon##TEST_FIXTURE_NAME {}

#define SINGLE_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunSingleThread_DelegatingRenderer) {   \
    RunTest(CompositorMode::SINGLE_THREADED, true);                 \
  }                                                                 \
  class SingleThreadDelegatingImplNeedsSemicolon##TEST_FIXTURE_NAME {}

#define SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME)            \
  SINGLE_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME); \
  SINGLE_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)

#define MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunMultiThread_DirectRenderer) {   \
    RunTest(CompositorMode::THREADED, false);                  \
  }                                                            \
  class MultiThreadDirectImplNeedsSemicolon##TEST_FIXTURE_NAME {}

#define MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunMultiThread_DelegatingRenderer) {   \
    RunTest(CompositorMode::THREADED, true);                       \
  }                                                                \
  class MultiThreadDelegatingImplNeedsSemicolon##TEST_FIXTURE_NAME {}

#define MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)            \
  MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME); \
  MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)

// The Remote mode tests don't need to run for delegated renderer.
#define REMOTE_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunRemote_DirectRenderer) {  \
    RunTest(CompositorMode::REMOTE, false);              \
  }                                                      \
  class RemoteDirectImplNeedsSemicolon##TEST_FIXTURE_NAME {}

#define SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  SINGLE_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME);                \
  MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME)

#define SINGLE_AND_MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  SINGLE_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME);                \
  MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)

#define SINGLE_AND_MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)            \
  SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME); \
  SINGLE_AND_MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)

// Some tests want to control when notify ready for activation occurs,
// but this is not supported in the single-threaded case.
#define MULTI_THREAD_BLOCKNOTIFY_TEST_F(TEST_FIXTURE_NAME) \
  MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)

#endif  // CC_TEST_LAYER_TREE_TEST_H_
