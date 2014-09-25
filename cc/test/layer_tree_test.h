// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_TEST_H_
#define CC_TEST_LAYER_TREE_TEST_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "cc/animation/animation_delegate.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace Webkit {
class WebGraphicsContext3D;
}

namespace cc {
class FakeLayerTreeHostClient;
class FakeOutputSurface;
class LayerImpl;
class LayerTreeHost;
class LayerTreeHostClient;
class LayerTreeHostImpl;
class TestContextProvider;
class TestWebGraphicsContext3D;

// Used by test stubs to notify the test when something interesting happens.
class TestHooks : public AnimationDelegate {
 public:
  TestHooks();
  virtual ~TestHooks();

  void ReadSettings(const LayerTreeSettings& settings);

  virtual void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                          const BeginFrameArgs& args) {}
  virtual void BeginMainFrameAbortedOnThread(LayerTreeHostImpl* host_impl,
                                             bool did_handle) {}
  virtual void BeginCommitOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void InitializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) {}
  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame_data,
      DrawResult draw_result);
  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl, bool result) {}
  virtual void SwapBuffersCompleteOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void UpdateVisibleTilesOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void AnimateLayers(LayerTreeHostImpl* host_impl,
                             base::TimeTicks monotonic_time) {}
  virtual void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                                    bool has_unfinished_animation) {}
  virtual void WillAnimateLayers(LayerTreeHostImpl* host_impl,
                                 base::TimeTicks monotonic_time) {}
  virtual void ApplyViewportDeltas(const gfx::Vector2d& scroll_delta,
                                   float scale,
                                   float top_controls_delta) {}
  virtual void BeginMainFrame(const BeginFrameArgs& args) {}
  virtual void WillBeginMainFrame() {}
  virtual void DidBeginMainFrame() {}
  virtual void Layout() {}
  virtual void DidInitializeOutputSurface() {}
  virtual void DidFailToInitializeOutputSurface() {}
  virtual void DidAddAnimation() {}
  virtual void WillCommit() {}
  virtual void DidCommit() {}
  virtual void DidCommitAndDrawFrame() {}
  virtual void DidCompleteSwapBuffers() {}
  virtual void DidDeferCommit() {}
  virtual void DidSetVisibleOnImplTree(LayerTreeHostImpl* host_impl,
                                       bool visible) {}
  virtual base::TimeDelta LowFrequencyAnimationInterval() const;

  // Hooks for SchedulerClient.
  virtual void ScheduledActionWillSendBeginMainFrame() {}
  virtual void ScheduledActionSendBeginMainFrame() {}
  virtual void ScheduledActionDrawAndSwapIfPossible() {}
  virtual void ScheduledActionAnimate() {}
  virtual void ScheduledActionCommit() {}
  virtual void ScheduledActionBeginOutputSurfaceCreation() {}

  // Implementation of AnimationDelegate:
  virtual void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                                      Animation::TargetProperty target_property)
      OVERRIDE {}
  virtual void NotifyAnimationFinished(
      base::TimeTicks monotonic_time,
      Animation::TargetProperty target_property) OVERRIDE {}

  virtual void RequestNewOutputSurface(bool fallback) = 0;
};

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
  virtual ~LayerTreeTest();

  virtual void EndTest();
  void EndTestAfterDelay(int delay_milliseconds);

  void PostAddAnimationToMainThread(Layer* layer_to_receive_animation);
  void PostAddInstantAnimationToMainThread(Layer* layer_to_receive_animation);
  void PostAddLongAnimationToMainThread(Layer* layer_to_receive_animation);
  void PostSetNeedsCommitToMainThread();
  void PostSetNeedsUpdateLayersToMainThread();
  void PostSetNeedsRedrawToMainThread();
  void PostSetNeedsRedrawRectToMainThread(const gfx::Rect& damage_rect);
  void PostSetVisibleToMainThread(bool visible);
  void PostSetNextCommitForcesRedrawToMainThread();

  void DoBeginTest();
  void Timeout();

 protected:
  LayerTreeTest();

  virtual void InitializeSettings(LayerTreeSettings* settings) {}

  void RealEndTest();

  virtual void DispatchAddAnimation(Layer* layer_to_receive_animation,
                                    double animation_duration);
  void DispatchSetNeedsCommit();
  void DispatchSetNeedsUpdateLayers();
  void DispatchSetNeedsRedraw();
  void DispatchSetNeedsRedrawRect(const gfx::Rect& damage_rect);
  void DispatchSetVisible(bool visible);
  void DispatchSetNextCommitForcesRedraw();
  void DispatchDidAddAnimation();

  virtual void AfterTest() = 0;
  virtual void WillBeginTest();
  virtual void BeginTest() = 0;
  virtual void SetupTree();

  virtual void RunTest(bool threaded,
                       bool delegating_renderer,
                       bool impl_side_painting);
  virtual void RunTestWithImplSidePainting();

  bool HasImplThread() { return proxy() ? proxy()->HasImplThread() : false; }
  base::SingleThreadTaskRunner* ImplThreadTaskRunner() {
    DCHECK(proxy());
    return proxy()->ImplThreadTaskRunner() ? proxy()->ImplThreadTaskRunner()
                                           : main_task_runner_.get();
  }
  base::SingleThreadTaskRunner* MainThreadTaskRunner() {
    return main_task_runner_.get();
  }
  Proxy* proxy() const {
    return layer_tree_host_ ? layer_tree_host_->proxy() : NULL;
  }

  bool TestEnded() const { return ended_; }

  LayerTreeHost* layer_tree_host() { return layer_tree_host_.get(); }
  bool delegating_renderer() const { return delegating_renderer_; }
  FakeOutputSurface* output_surface() { return output_surface_; }
  int LastCommittedSourceFrameNumber(LayerTreeHostImpl* impl) const;

  void DestroyLayerTreeHost();

  // By default, output surface recreation is synchronous.
  virtual void RequestNewOutputSurface(bool fallback) OVERRIDE;
  // Override this for pixel tests, where you need a real output surface.
  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback);
  // Override this for unit tests, which should not produce pixel output.
  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface(bool fallback);

  TestWebGraphicsContext3D* TestContext();


 private:
  LayerTreeSettings settings_;
  scoped_ptr<LayerTreeHostClientForTesting> client_;
  scoped_ptr<LayerTreeHost> layer_tree_host_;
  FakeOutputSurface* output_surface_;

  bool beginning_;
  bool end_when_begin_returns_;
  bool timed_out_;
  bool scheduled_;
  bool started_;
  bool ended_;
  bool delegating_renderer_;

  int timeout_seconds_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_ptr<base::Thread> impl_thread_;
  base::CancelableClosure timeout_;
  scoped_refptr<TestContextProvider> compositor_contexts_;
  base::WeakPtr<LayerTreeTest> main_thread_weak_ptr_;
  base::WeakPtrFactory<LayerTreeTest> weak_factory_;
};

}  // namespace cc

#define SINGLE_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunSingleThread_DirectRenderer) {   \
    RunTest(false, false, false);                               \
  }                                                             \
  class SingleThreadDirectNeedsSemicolon##TEST_FIXTURE_NAME {}

#define SINGLE_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunSingleThread_DelegatingRenderer) {   \
    RunTest(false, true, false);                                    \
  }                                                                 \
  class SingleThreadDelegatingNeedsSemicolon##TEST_FIXTURE_NAME {}

#define SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME)            \
  SINGLE_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME); \
  SINGLE_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)

#define MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME)        \
  TEST_F(TEST_FIXTURE_NAME, RunMultiThread_DirectRenderer_MainThreadPaint) { \
    RunTest(true, false, false);                                             \
  }

#define MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME)                 \
  MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME)                \
      TEST_F(TEST_FIXTURE_NAME, RunMultiThread_DirectRenderer_ImplSidePaint) { \
    RunTest(true, false, true);                                                \
  }                                                                            \
  class MultiThreadDirectNeedsSemicolon##TEST_FIXTURE_NAME {}

#define MULTI_THREAD_DELEGATING_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME,                                               \
         RunMultiThread_DelegatingRenderer_MainThreadPaint) {             \
    RunTest(true, true, false);                                           \
  }

#define MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)          \
  MULTI_THREAD_DELEGATING_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME) TEST_F( \
      TEST_FIXTURE_NAME, RunMultiThread_DelegatingRenderer_ImplSidePaint) { \
    RunTest(true, true, true);                                              \
  }                                                                         \
  class MultiThreadDelegatingNeedsSemicolon##TEST_FIXTURE_NAME {}

#define MULTI_THREAD_NOIMPL_TEST_F(TEST_FIXTURE_NAME)            \
  MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME); \
  MULTI_THREAD_DELEGATING_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME)

#define MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)            \
  MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME); \
  MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)

#define SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F( \
    TEST_FIXTURE_NAME)                                         \
  SINGLE_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME);     \
  MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME)

#define SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  SINGLE_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME);                \
  MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME)

#define SINGLE_AND_MULTI_THREAD_DELEGATING_RENDERER_NOIMPL_TEST_F( \
    TEST_FIXTURE_NAME)                                             \
  SINGLE_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME);     \
  MULTI_THREAD_DELEGATING_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME)

#define SINGLE_AND_MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME) \
  SINGLE_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME);                \
  MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)

#define SINGLE_AND_MULTI_THREAD_NOIMPL_TEST_F(TEST_FIXTURE_NAME)            \
  SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME); \
  SINGLE_AND_MULTI_THREAD_DELEGATING_RENDERER_NOIMPL_TEST_F(TEST_FIXTURE_NAME)

#define SINGLE_AND_MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)            \
  SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TEST_FIXTURE_NAME); \
  SINGLE_AND_MULTI_THREAD_DELEGATING_RENDERER_TEST_F(TEST_FIXTURE_NAME)

#endif  // CC_TEST_LAYER_TREE_TEST_H_
