// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_test.h"

#include "cc/animation/animation.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/timing_function.h"
#include "cc/base/thread_impl.h"
#include "cc/input/input_handler.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "ui/gfx/size_conversions.h"

using namespace WebKit;

namespace cc {

TestHooks::TestHooks() {
  fake_client_.reset(
      new FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D));
}

TestHooks::~TestHooks() {}

bool TestHooks::PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                      LayerTreeHostImpl::FrameData* frame_data,
                                      bool result) {
  return true;
}

bool TestHooks::CanActivatePendingTree() {
  return true;
}

scoped_ptr<OutputSurface> TestHooks::CreateOutputSurface() {
  return CreateFakeOutputSurface();
}

scoped_refptr<cc::ContextProvider> TestHooks::
    OffscreenContextProviderForMainThread() {
  return fake_client_->OffscreenContextProviderForMainThread();
}

scoped_refptr<cc::ContextProvider> TestHooks::
    OffscreenContextProviderForCompositorThread() {
  return fake_client_->OffscreenContextProviderForCompositorThread();
}

// Adapts LayerTreeHostImpl for test. Runs real code, then invokes test hooks.
class LayerTreeHostImplForTesting : public LayerTreeHostImpl {
 public:
  typedef std::vector<LayerImpl*> LayerList;

  static scoped_ptr<LayerTreeHostImplForTesting> Create(
      TestHooks* test_hooks,
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* host_impl_client,
      Proxy* proxy,
      RenderingStatsInstrumentation* stats_instrumentation) {
    return make_scoped_ptr(
        new LayerTreeHostImplForTesting(test_hooks,
                                        settings,
                                        host_impl_client,
                                        proxy,
                                        stats_instrumentation));
  }

 protected:
  LayerTreeHostImplForTesting(
      TestHooks* test_hooks,
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* host_impl_client,
      Proxy* proxy,
      RenderingStatsInstrumentation* stats_instrumentation)
      : LayerTreeHostImpl(settings,
                          host_impl_client,
                          proxy,
                          stats_instrumentation),
        test_hooks_(test_hooks) {}

  virtual void BeginCommit() OVERRIDE {
    LayerTreeHostImpl::BeginCommit();
    test_hooks_->BeginCommitOnThread(this);
  }

  virtual void CommitComplete() OVERRIDE {
    LayerTreeHostImpl::CommitComplete();
    test_hooks_->CommitCompleteOnThread(this);

    if (!settings().impl_side_painting)
      test_hooks_->TreeActivatedOnThread(this);
  }

  virtual bool PrepareToDraw(FrameData* frame) OVERRIDE {
    bool result = LayerTreeHostImpl::PrepareToDraw(frame);
    if (!test_hooks_->PrepareToDrawOnThread(this, frame, result))
      result = false;
    return result;
  }

  virtual void DrawLayers(FrameData* frame,
                          base::TimeTicks frame_begin_time) OVERRIDE {
    LayerTreeHostImpl::DrawLayers(frame, frame_begin_time);
    test_hooks_->DrawLayersOnThread(this);
  }

  virtual bool SwapBuffers() OVERRIDE {
    bool result = LayerTreeHostImpl::SwapBuffers();
    test_hooks_->SwapBuffersOnThread(this, result);
    return result;
  }

  virtual bool ActivatePendingTreeIfNeeded() OVERRIDE {
    if (!pending_tree())
      return false;

    if (!test_hooks_->CanActivatePendingTree())
      return false;

    bool activated = LayerTreeHostImpl::ActivatePendingTreeIfNeeded();
    if (activated)
      test_hooks_->TreeActivatedOnThread(this);
    return activated;
  }

  virtual bool InitializeRenderer(scoped_ptr<OutputSurface> output_surface)
      OVERRIDE {
    bool success = LayerTreeHostImpl::InitializeRenderer(output_surface.Pass());
    test_hooks_->InitializedRendererOnThread(this, success);
    return success;
  }

  virtual void SetVisible(bool visible) OVERRIDE {
    LayerTreeHostImpl::SetVisible(visible);
    test_hooks_->DidSetVisibleOnImplTree(this, visible);
  }

  virtual void AnimateLayers(base::TimeTicks monotonic_time,
                             base::Time wall_clock_time) OVERRIDE {
    test_hooks_->WillAnimateLayers(this, monotonic_time);
    LayerTreeHostImpl::AnimateLayers(monotonic_time, wall_clock_time);
    test_hooks_->AnimateLayers(this, monotonic_time);
  }

  virtual void UpdateAnimationState() OVERRIDE {
    LayerTreeHostImpl::UpdateAnimationState();
    bool has_unfinished_animation = false;
    AnimationRegistrar::AnimationControllerMap::const_iterator iter =
        active_animation_controllers().begin();
    for (; iter != active_animation_controllers().end(); ++iter) {
      if (iter->second->HasActiveAnimation()) {
        has_unfinished_animation = true;
        break;
      }
    }
    test_hooks_->UpdateAnimationState(this, has_unfinished_animation);
  }

  virtual base::TimeDelta LowFrequencyAnimationInterval() const OVERRIDE {
    return base::TimeDelta::FromMilliseconds(16);
  }

 private:
  TestHooks* test_hooks_;
};

// Adapts LayerTreeHost for test. Injects LayerTreeHostImplForTesting.
class LayerTreeHostForTesting : public cc::LayerTreeHost {
 public:
  static scoped_ptr<LayerTreeHostForTesting> Create(
      TestHooks* test_hooks,
      cc::LayerTreeHostClient* host_client,
      const cc::LayerTreeSettings& settings,
      scoped_ptr<cc::Thread> impl_thread) {
    scoped_ptr<LayerTreeHostForTesting> layer_tree_host(
        new LayerTreeHostForTesting(test_hooks, host_client, settings));
    bool success = layer_tree_host->Initialize(impl_thread.Pass());
    EXPECT_TRUE(success);
    return layer_tree_host.Pass();
  }

  virtual scoped_ptr<cc::LayerTreeHostImpl> CreateLayerTreeHostImpl(
      cc::LayerTreeHostImplClient* host_impl_client) OVERRIDE {
    return LayerTreeHostImplForTesting::Create(
        test_hooks_,
        settings(),
        host_impl_client,
        proxy(),
        rendering_stats_instrumentation()).PassAs<cc::LayerTreeHostImpl>();
  }

  virtual void SetNeedsCommit() OVERRIDE {
    if (!test_started_)
      return;
    LayerTreeHost::SetNeedsCommit();
  }

  void set_test_started(bool started) { test_started_ = started; }

  virtual void DidDeferCommit() OVERRIDE {
    test_hooks_->DidDeferCommit();
  }

 private:
  LayerTreeHostForTesting(TestHooks* test_hooks,
                          cc::LayerTreeHostClient* client,
                          const cc::LayerTreeSettings& settings)
      : LayerTreeHost(client, settings),
        test_hooks_(test_hooks),
        test_started_(false) {}

  TestHooks* test_hooks_;
  bool test_started_;
};

// Implementation of LayerTreeHost callback interface.
class LayerTreeHostClientForTesting : public LayerTreeHostClient {
 public:
  static scoped_ptr<LayerTreeHostClientForTesting> Create(
      TestHooks* test_hooks) {
    return make_scoped_ptr(new LayerTreeHostClientForTesting(test_hooks));
  }
  virtual ~LayerTreeHostClientForTesting() {}

  virtual void WillBeginFrame() OVERRIDE {}

  virtual void DidBeginFrame() OVERRIDE {}

  virtual void Animate(double monotonic_time) OVERRIDE {
    test_hooks_->Animate(base::TimeTicks::FromInternalValue(
        monotonic_time * base::Time::kMicrosecondsPerSecond));
  }

  virtual void Layout() OVERRIDE {
    test_hooks_->Layout();
  }

  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float scale) OVERRIDE {
    test_hooks_->ApplyScrollAndScale(scroll_delta, scale);
  }

  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE {
    return test_hooks_->CreateOutputSurface();
  }

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    test_hooks_->DidRecreateOutputSurface(succeeded);
  }

  virtual void WillRetryRecreateOutputSurface() OVERRIDE {
    test_hooks_->WillRetryRecreateOutputSurface();
  }

  virtual scoped_ptr<InputHandler> CreateInputHandler() OVERRIDE {
    return scoped_ptr<InputHandler>();
  }

  virtual void WillCommit() OVERRIDE {}

  virtual void DidCommit() OVERRIDE {
    test_hooks_->DidCommit();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    test_hooks_->DidCommitAndDrawFrame();
  }

  virtual void DidCompleteSwapBuffers() OVERRIDE {}

  virtual void ScheduleComposite() OVERRIDE {
    test_hooks_->ScheduleComposite();
  }

  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE {
    return test_hooks_->OffscreenContextProviderForMainThread();
  }

  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE {
    return test_hooks_->OffscreenContextProviderForCompositorThread();
  }

 private:
  explicit LayerTreeHostClientForTesting(TestHooks* test_hooks)
      : test_hooks_(test_hooks) {}

  TestHooks* test_hooks_;
};

LayerTreeTest::LayerTreeTest()
    : beginning_(false),
      end_when_begin_returns_(false),
      timed_out_(false),
      scheduled_(false),
      schedule_when_set_visible_true_(false),
      started_(false),
      ended_(false),
      impl_thread_(NULL),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  main_thread_weak_ptr_ = weak_factory_.GetWeakPtr();
}

LayerTreeTest::~LayerTreeTest() {}

void LayerTreeTest::EndTest() {
  // For the case where we endTest during BeginTest(), set a flag to indicate
  // that the test should end the second beginTest regains control.
  if (beginning_)
    end_when_begin_returns_ = true;
  else
    proxy()->MainThread()->PostTask(
        base::Bind(&LayerTreeTest::RealEndTest, main_thread_weak_ptr_));
}

void LayerTreeTest::EndTestAfterDelay(int delay_milliseconds) {
  proxy()->MainThread()->PostTask(
      base::Bind(&LayerTreeTest::EndTest, main_thread_weak_ptr_));
}

void LayerTreeTest::PostAddAnimationToMainThread(
    Layer* layer_to_receive_animation) {
  proxy()->MainThread()->PostTask(
      base::Bind(&LayerTreeTest::DispatchAddAnimation,
                 main_thread_weak_ptr_,
                 base::Unretained(layer_to_receive_animation)));
}

void LayerTreeTest::PostAddInstantAnimationToMainThread() {
  proxy()->MainThread()->PostTask(
      base::Bind(&LayerTreeTest::DispatchAddInstantAnimation,
                 main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsCommitToMainThread() {
  proxy()->MainThread()->PostTask(
      base::Bind(&LayerTreeTest::DispatchSetNeedsCommit,
                 main_thread_weak_ptr_));
}

void LayerTreeTest::PostAcquireLayerTextures() {
  proxy()->MainThread()->PostTask(
      base::Bind(&LayerTreeTest::DispatchAcquireLayerTextures,
                 main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsRedrawToMainThread() {
  proxy()->MainThread()->PostTask(
      base::Bind(&LayerTreeTest::DispatchSetNeedsRedraw,
                 main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetVisibleToMainThread(bool visible) {
  proxy()->MainThread()->PostTask(
      base::Bind(&LayerTreeTest::DispatchSetVisible,
                 main_thread_weak_ptr_,
                 visible));
}

void LayerTreeTest::DoBeginTest() {
  client_ = LayerTreeHostClientForTesting::Create(this);

  scoped_ptr<cc::Thread> impl_ccthread(NULL);
  if (impl_thread_) {
    impl_ccthread = cc::ThreadImpl::CreateForDifferentThread(
        impl_thread_->message_loop_proxy());
  }
  layer_tree_host_ = LayerTreeHostForTesting::Create(this,
                                                     client_.get(),
                                                     settings_,
                                                     impl_ccthread.Pass());
  ASSERT_TRUE(layer_tree_host_);

  started_ = true;
  beginning_ = true;
  SetupTree();
  layer_tree_host_->SetSurfaceReady();
  BeginTest();
  beginning_ = false;
  if (end_when_begin_returns_)
    RealEndTest();

  // Allow commits to happen once BeginTest() has had a chance to post tasks
  // so that those tasks will happen before the first commit.
  if (layer_tree_host_) {
    static_cast<LayerTreeHostForTesting*>(layer_tree_host_.get())->
        set_test_started(true);
  }
}

void LayerTreeTest::SetupTree() {
  if (!layer_tree_host_->root_layer()) {
    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(1, 1));
    layer_tree_host_->SetRootLayer(root_layer);
  }

  gfx::Size root_bounds = layer_tree_host_->root_layer()->bounds();
  gfx::Size device_root_bounds = gfx::ToCeiledSize(
      gfx::ScaleSize(root_bounds, layer_tree_host_->device_scale_factor()));
  layer_tree_host_->SetViewportSize(root_bounds, device_root_bounds);
}

void LayerTreeTest::Timeout() {
  timed_out_ = true;
  EndTest();
}

void LayerTreeTest::ScheduleComposite() {
  if (!started_ || scheduled_)
    return;
  scheduled_ = true;
  proxy()->MainThread()->PostTask(
      base::Bind(&LayerTreeTest::DispatchComposite, main_thread_weak_ptr_));
}

void LayerTreeTest::RealEndTest() {
  ended_ = true;

  if (layer_tree_host_ && proxy()->CommitPendingForTesting()) {
    proxy()->MainThread()->PostTask(
        base::Bind(&LayerTreeTest::RealEndTest, main_thread_weak_ptr_));
    return;
  }
        
  MessageLoop::current()->Quit();
}

void LayerTreeTest::DispatchAddInstantAnimation() {
  DCHECK(!proxy() || proxy()->IsMainThread());

  if (layer_tree_host_.get() && layer_tree_host_->root_layer()) {
    AddOpacityTransitionToLayer(layer_tree_host_->root_layer(),
                                0,
                                0,
                                0.5,
                                false);
  }
}

void LayerTreeTest::DispatchAddAnimation(Layer* layer_to_receive_animation) {
  DCHECK(!proxy() || proxy()->IsMainThread());

  if (layer_to_receive_animation)
    AddOpacityTransitionToLayer(layer_to_receive_animation, 10, 0, 0.5, true);
}

void LayerTreeTest::DispatchSetNeedsCommit() {
  DCHECK(!proxy() || proxy()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->SetNeedsCommit();
}

void LayerTreeTest::DispatchAcquireLayerTextures() {
  DCHECK(!proxy() || proxy()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->AcquireLayerTextures();
}

void LayerTreeTest::DispatchSetNeedsRedraw() {
  DCHECK(!proxy() || proxy()->IsMainThread());

  if (layer_tree_host_)
    layer_tree_host_->SetNeedsRedraw();
}

void LayerTreeTest::DispatchSetVisible(bool visible) {
  DCHECK(!proxy() || proxy()->IsMainThread());

  if (!layer_tree_host_)
    return;

  layer_tree_host_->SetVisible(visible);

  // If the LTH is being made visible and a previous ScheduleComposite() was
  // deferred because the LTH was not visible, re-schedule the composite now.
  if (layer_tree_host_->visible() && schedule_when_set_visible_true_)
    ScheduleComposite();
}

void LayerTreeTest::DispatchComposite() {
  scheduled_ = false;

  if (!layer_tree_host_)
    return;

  // If the LTH is not visible, defer the composite until the LTH is made
  // visible.
  if (!layer_tree_host_->visible()) {
    schedule_when_set_visible_true_ = true;
    return;
  }

  schedule_when_set_visible_true_ = false;
  layer_tree_host_->Composite(base::TimeTicks::Now());
}

void LayerTreeTest::RunTest(bool threaded) {
  if (threaded) {
    impl_thread_.reset(new base::Thread("LayerTreeTest"));
    ASSERT_TRUE(impl_thread_->Start());
  }

  main_ccthread_ = cc::ThreadImpl::CreateForCurrentThread();

  InitializeSettings(&settings_);

  main_ccthread_->PostTask(
      base::Bind(&LayerTreeTest::DoBeginTest, base::Unretained(this)));
  timeout_.Reset(base::Bind(&LayerTreeTest::Timeout, base::Unretained(this)));
  main_ccthread_->PostDelayedTask(timeout_.callback(),
                                  base::TimeDelta::FromSeconds(5));
  MessageLoop::current()->Run();
  if (layer_tree_host_ && layer_tree_host_->root_layer())
    layer_tree_host_->root_layer()->SetLayerTreeHost(NULL);
  layer_tree_host_.reset();

  timeout_.Cancel();

  ASSERT_FALSE(layer_tree_host_.get());
  client_.reset();
  if (timed_out_) {
    FAIL() << "Test timed out";
    return;
  }
  AfterTest();
}

}  // namespace cc
