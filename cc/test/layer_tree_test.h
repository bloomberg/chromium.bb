// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_TEST_COMMON_H_
#define CC_TEST_LAYER_TREE_TEST_COMMON_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "cc/base/thread.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimationDelegate.h"

namespace Webkit {
class WebGraphicsContext3D;
}

namespace cc {
class FakeLayerTreeHostClient;
class LayerImpl;
class LayerTreeHost;
class LayerTreeHostClient;
class LayerTreeHostImpl;

// Used by test stubs to notify the test when something interesting happens.
class TestHooks : public WebKit::WebAnimationDelegate {
 public:
  TestHooks();
  virtual ~TestHooks();

  virtual void BeginCommitOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void TreeActivatedOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void InitializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) {}
  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result);
  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl, bool result) {}
  virtual void AnimateLayers(LayerTreeHostImpl* host_impl,
                             base::TimeTicks monotonic_time) {}
  virtual void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                                    bool has_unfinished_animation) {}
  virtual void WillAnimateLayers(LayerTreeHostImpl* host_impl,
                                 base::TimeTicks monotonic_time) {}
  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float scale) {}
  virtual void Animate(base::TimeTicks monotonic_time) {}
  virtual void Layout() {}
  virtual void DidRecreateOutputSurface(bool succeeded) {}
  virtual void WillRetryRecreateOutputSurface() {}
  virtual void DidAddAnimation() {}
  virtual void DidCommit() {}
  virtual void DidCommitAndDrawFrame() {}
  virtual void ScheduleComposite() {}
  virtual void DidDeferCommit() {}
  virtual bool CanActivatePendingTree();
  virtual void DidSetVisibleOnImplTree(LayerTreeHostImpl* host_impl,
                                       bool visible) {}

  // Implementation of WebAnimationDelegate
  virtual void notifyAnimationStarted(double time) OVERRIDE {}
  virtual void notifyAnimationFinished(double time) OVERRIDE {}

  virtual scoped_ptr<OutputSurface> CreateOutputSurface();

  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread();
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread();

 private:
  scoped_ptr<FakeLayerTreeHostClient> fake_client_;
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

  virtual void AfterTest() = 0;
  virtual void BeginTest() = 0;
  virtual void SetupTree();

  void EndTest();
  void EndTestAfterDelay(int delay_milliseconds);

  void PostAddAnimationToMainThread(Layer* layer_to_receive_animation);
  void PostAddInstantAnimationToMainThread();
  void PostSetNeedsCommitToMainThread();
  void PostAcquireLayerTextures();
  void PostSetNeedsRedrawToMainThread();
  void PostSetVisibleToMainThread(bool visible);

  void DoBeginTest();
  void Timeout();

 protected:
  LayerTreeTest();

  virtual void InitializeSettings(LayerTreeSettings* settings) {}

  virtual void ScheduleComposite() OVERRIDE;

  void RealEndTest();

  void DispatchAddInstantAnimation();
  void DispatchAddAnimation(Layer* layer_to_receive_animation);
  void DispatchSetNeedsCommit();
  void DispatchAcquireLayerTextures();
  void DispatchSetNeedsRedraw();
  void DispatchSetVisible(bool visible);
  void DispatchComposite();
  void DispatchDidAddAnimation();

  virtual void RunTest(bool threaded);

  Thread* ImplThread() { return proxy() ? proxy()->ImplThread() : NULL; }
  Proxy* proxy() const {
    return layer_tree_host_ ? layer_tree_host_->proxy() : NULL;
  }

  bool TestEnded() const { return ended_; }

  LayerTreeHost* layer_tree_host() { return layer_tree_host_.get(); }

 private:
  LayerTreeSettings settings_;
  scoped_ptr<LayerTreeHostClientForTesting> client_;
  scoped_ptr<LayerTreeHost> layer_tree_host_;

  bool beginning_;
  bool end_when_begin_returns_;
  bool timed_out_;
  bool scheduled_;
  bool schedule_when_set_visible_true_;
  bool started_;
  bool ended_;

  scoped_ptr<Thread> main_ccthread_;
  scoped_ptr<base::Thread> impl_thread_;
  base::CancelableClosure timeout_;
  base::WeakPtr<LayerTreeTest> main_thread_weak_ptr_;
  base::WeakPtrFactory<LayerTreeTest> weak_factory_;
};

}  // namespace cc

#define SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunSingleThread) {  \
    RunTest(false);                             \
  }

#define MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)  \
  TEST_F(TEST_FIXTURE_NAME, RunMultiThread) {   \
    RunTest(true);                              \
  }

#define SINGLE_AND_MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME) \
  SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME)                 \
  MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)

#endif  // CC_TEST_LAYER_TREE_TEST_COMMON_H_
