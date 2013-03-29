// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/animation/animation_curve.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/timing_function.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_test.h"

namespace cc {
namespace {

class LayerTreeHostAnimationTest : public LayerTreeTest {
 public:
  virtual void SetupTree() OVERRIDE {
    LayerTreeTest::SetupTree();
    layer_tree_host()->root_layer()->set_layer_animation_delegate(this);
  }
};

// Makes sure that SetNeedsAnimate does not cause the CommitRequested() state to
// be set.
class LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested()
      : num_commits_(0) {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void Animate(base::TimeTicks monotonic_time) OVERRIDE {
    // We skip the first commit becasue its the commit that populates the
    // impl thread with a tree. After the second commit, the test is done.
    if (num_commits_ != 1)
      return;

    layer_tree_host()->SetNeedsAnimate();
    // Right now, CommitRequested is going to be true, because during
    // BeginFrame, we force CommitRequested to true to prevent requests from
    // hitting the impl thread. But, when the next DidCommit happens, we should
    // verify that CommitRequested has gone back to false.
  }

  virtual void DidCommit() OVERRIDE {
    if (!num_commits_) {
      EXPECT_FALSE(layer_tree_host()->CommitRequested());
      layer_tree_host()->SetNeedsAnimate();
      EXPECT_FALSE(layer_tree_host()->CommitRequested());
    }

    // Verifies that the SetNeedsAnimate we made in ::Animate did not
    // trigger CommitRequested.
    EXPECT_FALSE(layer_tree_host()->CommitRequested());
    EndTest();
    num_commits_++;
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested);

// Trigger a frame with SetNeedsCommit. Then, inside the resulting animate
// callback, requet another frame using SetNeedsAnimate. End the test when
// animate gets called yet-again, indicating that the proxy is correctly
// handling the case where SetNeedsAnimate() is called inside the begin frame
// flow.
class LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback()
      : num_animates_(0) {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void Animate(base::TimeTicks) OVERRIDE {
    if (!num_animates_) {
      layer_tree_host()->SetNeedsAnimate();
      num_animates_++;
      return;
    }
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_animates_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback);

// Add a layer animation and confirm that
// LayerTreeHostImpl::updateAnimationState does get called and continues to
// get called.
class LayerTreeHostAnimationTestAddAnimation
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAddAnimation()
      : num_animates_(0)
      , received_animation_started_notification_(false)
      , start_time_(0.0) {
  }

  virtual void BeginTest() OVERRIDE {
    PostAddInstantAnimationToMainThread();
  }

  virtual void UpdateAnimationState(
      LayerTreeHostImpl* impl_host,
      bool has_unfinished_animation) OVERRIDE {
    if (!num_animates_) {
      // The animation had zero duration so LayerTreeHostImpl should no
      // longer need to animate its layers.
      EXPECT_FALSE(has_unfinished_animation);
      num_animates_++;
      return;
    }

    if (received_animation_started_notification_) {
      EXPECT_LT(0.0, start_time_);
      EndTest();
    }
  }

  virtual void notifyAnimationStarted(double wall_clock_time) OVERRIDE {
    received_animation_started_notification_ = true;
    start_time_ = wall_clock_time;
    if (num_animates_) {
      EXPECT_LT(0.0, start_time_);
      EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_animates_;
  bool received_animation_started_notification_;
  double start_time_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAddAnimation);

// Add a layer animation to a layer, but continually fail to draw. Confirm that
// after a while, we do eventually force a draw.
class LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws()
      : started_animating_(false) {}

  virtual void BeginTest() OVERRIDE {
    PostAddAnimationToMainThread(layer_tree_host()->root_layer());
  }

  virtual void AnimateLayers(
      LayerTreeHostImpl* host_impl,
      base::TimeTicks monotonic_time) OVERRIDE {
    started_animating_ = true;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* tree_impl) OVERRIDE {
    if (started_animating_)
      EndTest();
  }

  virtual bool PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame,
      bool result) OVERRIDE {
    return false;
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  bool started_animating_;
};

// Starvation can only be an issue with the MT compositor.
MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws);

// Ensures that animations continue to be ticked when we are backgrounded.
class LayerTreeHostAnimationTestTickAnimationWhileBackgrounded
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestTickAnimationWhileBackgrounded()
      : num_animates_(0) {}

  virtual void BeginTest() OVERRIDE {
    PostAddAnimationToMainThread(layer_tree_host()->root_layer());
  }

  // Use WillAnimateLayers to set visible false before the animation runs and
  // causes a commit, so we block the second visible animate in single-thread
  // mode.
  virtual void WillAnimateLayers(
      LayerTreeHostImpl* host_impl,
      base::TimeTicks monotonic_time) OVERRIDE {
    if (num_animates_ < 2) {
      if (!num_animates_) {
        // We have a long animation running. It should continue to tick even
        // if we are not visible.
        PostSetVisibleToMainThread(false);
      }
      num_animates_++;
      return;
    }
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_animates_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestTickAnimationWhileBackgrounded);

// Ensures that animations continue to be ticked when we are backgrounded.
class LayerTreeHostAnimationTestAddAnimationWithTimingFunction
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAddAnimationWithTimingFunction() {}

  virtual void BeginTest() OVERRIDE {
    PostAddAnimationToMainThread(layer_tree_host()->root_layer());
  }

  virtual void AnimateLayers(
      LayerTreeHostImpl* host_impl,
      base::TimeTicks monotonic_time) OVERRIDE {
    LayerAnimationController* controller =
        layer_tree_host()->root_layer()->layer_animation_controller();
    Animation* animation =
        controller->GetAnimation(0, Animation::Opacity);
    if (!animation)
      return;

    const FloatAnimationCurve* curve =
        animation->curve()->ToFloatAnimationCurve();
    float start_opacity = curve->GetValue(0.0);
    float end_opacity = curve->GetValue(curve->Duration());
    float linearly_interpolated_opacity =
        0.25f * end_opacity + 0.75f * start_opacity;
    double time = curve->Duration() * 0.25;
    // If the linear timing function associated with this animation was not
    // picked up, then the linearly interpolated opacity would be different
    // because of the default ease timing function.
    EXPECT_FLOAT_EQ(linearly_interpolated_opacity, curve->GetValue(time));

    LayerAnimationController* controller_impl =
        host_impl->RootLayer()->layer_animation_controller();
    Animation* animation_impl =
        controller_impl->GetAnimation(0, Animation::Opacity);

    controller->RemoveAnimation(animation->id());
    controller_impl->RemoveAnimation(animation_impl->id());
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAddAnimationWithTimingFunction);

// Ensures that main thread animations have their start times synchronized with
// impl thread animations.
class LayerTreeHostAnimationTestSynchronizeAnimationStartTimes
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSynchronizeAnimationStartTimes()
      : main_start_time_(-1.0),
        impl_start_time_(-1.0) {}

  virtual void BeginTest() OVERRIDE {
    PostAddAnimationToMainThread(layer_tree_host()->root_layer());
  }

  virtual void notifyAnimationStarted(double time) OVERRIDE {
    LayerAnimationController* controller =
        layer_tree_host()->root_layer()->layer_animation_controller();
    Animation* animation =
        controller->GetAnimation(0, Animation::Opacity);
    main_start_time_ = animation->start_time();
    controller->RemoveAnimation(animation->id());

    if (impl_start_time_ > 0.0)
      EndTest();
  }

  virtual void UpdateAnimationState(
      LayerTreeHostImpl* impl_host,
      bool has_unfinished_animation) OVERRIDE {
    LayerAnimationController* controller =
        impl_host->RootLayer()->layer_animation_controller();
    Animation* animation =
        controller->GetAnimation(0, Animation::Opacity);
    if (!animation)
      return;

    impl_start_time_ = animation->start_time();
    controller->RemoveAnimation(animation->id());

    if (main_start_time_ > 0.0)
      EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_FLOAT_EQ(impl_start_time_, main_start_time_);
  }

 private:
  double main_start_time_;
  double impl_start_time_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSynchronizeAnimationStartTimes);

// Ensures that notify animation finished is called.
class LayerTreeHostAnimationTestAnimationFinishedEvents
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationFinishedEvents() {}

  virtual void BeginTest() OVERRIDE {
    PostAddInstantAnimationToMainThread();
  }

  virtual void notifyAnimationFinished(double time) OVERRIDE {
    LayerAnimationController* controller =
        layer_tree_host()->root_layer()->layer_animation_controller();
    Animation* animation =
        controller->GetAnimation(0, Animation::Opacity);
    if (animation)
      controller->RemoveAnimation(animation->id());
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAnimationFinishedEvents);

// Ensures that when opacity is being animated, this value does not cause the
// subtree to be skipped.
class LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity()
      : update_check_layer_(FakeContentLayer::Create(&client_)) {
  }

  virtual void SetupTree() OVERRIDE {
    update_check_layer_->SetOpacity(0.f);
    layer_tree_host()->SetRootLayer(update_check_layer_);
    LayerTreeHostAnimationTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostAddAnimationToMainThread(update_check_layer_.get());
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* tree_impl) OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Update() should have been called once, proving that the layer was not
    // skipped.
    EXPECT_EQ(1u, update_check_layer_->update_count());

    // clear update_check_layer_ so LayerTreeHost dies.
    update_check_layer_ = NULL;
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> update_check_layer_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity);

// Layers added to tree with existing active animations should have the
// animation correctly recognized.
class LayerTreeHostAnimationTestLayerAddedWithAnimation
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestLayerAddedWithAnimation() {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    if (layer_tree_host()->commit_number() == 1) {
      scoped_refptr<Layer> layer = Layer::Create();
      layer->set_layer_animation_delegate(this);

      // Any valid AnimationCurve will do here.
      scoped_ptr<AnimationCurve> curve(EaseTimingFunction::Create());
      scoped_ptr<Animation> animation(
          Animation::Create(curve.Pass(), 1, 1,
                                  Animation::Opacity));
      layer->layer_animation_controller()->AddAnimation(animation.Pass());

      // We add the animation *before* attaching the layer to the tree.
      layer_tree_host()->root_layer()->AddChild(layer);
    }
  }

  virtual void AnimateLayers(
      LayerTreeHostImpl* impl_host,
      base::TimeTicks monotonic_time) OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestLayerAddedWithAnimation);

class LayerTreeHostAnimationTestCompositeAndReadbackAnimateCount
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCompositeAndReadbackAnimateCount()
      : animated_commit_(-1) {
  }

  virtual void Animate(base::TimeTicks) OVERRIDE {
    // We shouldn't animate on the CompositeAndReadback-forced commit, but we
    // should for the SetNeedsCommit-triggered commit.
    animated_commit_ = layer_tree_host()->commit_number();
    EXPECT_NE(2, animated_commit_);
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->commit_number()) {
      case 1:
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2: {
        char pixels[4];
        layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
        break;
      }
      case 3:
        // This is finishing the readback's commit.
        break;
      case 4:
        // This is finishing the followup commit.
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(3, animated_commit_);
  }

 private:
  int animated_commit_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCompositeAndReadbackAnimateCount);

class LayerTreeHostAnimationTestContinuousAnimate
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestContinuousAnimate()
      : num_commit_complete_(0),
        num_draw_layers_(0) {
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void Animate(base::TimeTicks) OVERRIDE {
    if (num_draw_layers_ == 2)
      return;
    layer_tree_host()->SetNeedsAnimate();
  }

  virtual void Layout() OVERRIDE {
    layer_tree_host()->root_layer()->SetNeedsDisplay();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* tree_impl) OVERRIDE {
    if (num_draw_layers_ == 1)
      num_commit_complete_++;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draw_layers_++;
    if (num_draw_layers_ == 2)
      EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Check that we didn't commit twice between first and second draw.
    EXPECT_EQ(1, num_commit_complete_);
  }

 private:
  int num_commit_complete_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestContinuousAnimate);

}  // namespace
}  // namespace cc
