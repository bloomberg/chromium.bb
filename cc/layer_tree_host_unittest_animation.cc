// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "cc/animation_curve.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_impl.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/timing_function.h"

namespace cc {
namespace {

class LayerTreeHostAnimationTest : public ThreadedTest {
 public:
  virtual void setupTree() OVERRIDE {
    ThreadedTest::setupTree();
    m_layerTreeHost->rootLayer()->setLayerAnimationDelegate(this);
  }
};

// Makes sure that setNeedsAnimate does not cause the commitRequested() state to
// be set.
class LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested()
      : num_commits_(0) {
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void animate(base::TimeTicks monotonicTime) OVERRIDE {
    // We skip the first commit becasue its the commit that populates the
    // impl thread with a tree. After the second commit, the test is done.
    if (num_commits_ != 1)
      return;

    m_layerTreeHost->setNeedsAnimate();
    // Right now, commitRequested is going to be true, because during
    // beginFrame, we force commitRequested to true to prevent requests from
    // hitting the impl thread. But, when the next didCommit happens, we should
    // verify that commitRequested has gone back to false.
  }

  virtual void didCommit() OVERRIDE {
    if (!num_commits_) {
      EXPECT_FALSE(m_layerTreeHost->commitRequested());
      m_layerTreeHost->setNeedsAnimate();
      EXPECT_FALSE(m_layerTreeHost->commitRequested());
    }

    // Verifies that the setNeedsAnimate we made in ::animate did not
    // trigger commitRequested.
    EXPECT_FALSE(m_layerTreeHost->commitRequested());
    endTest();
    num_commits_++;
  }

  virtual void afterTest() OVERRIDE {}

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested)

// Trigger a frame with setNeedsCommit. Then, inside the resulting animate
// callback, requet another frame using setNeedsAnimate. End the test when
// animate gets called yet-again, indicating that the proxy is correctly
// handling the case where setNeedsAnimate() is called inside the begin frame
// flow.
class LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback()
      : num_animates_(0) {
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void animate(base::TimeTicks) OVERRIDE {
    if (!num_animates_) {
      m_layerTreeHost->setNeedsAnimate();
      num_animates_++;
      return;
    }
    endTest();
  }

  virtual void afterTest() OVERRIDE {}

 private:
  int num_animates_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback)

// Add a layer animation and confirm that LayerTreeHostImpl::animateLayers does
// get called and continues to get called.
class LayerTreeHostAnimationTestAddAnimation :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAddAnimation()
      : num_animates_(0)
      , received_animation_started_notification_(false)
      , start_time_(0) {
  }

  virtual void beginTest() OVERRIDE {
    postAddInstantAnimationToMainThread();
  }

  virtual void animateLayers(
      LayerTreeHostImpl* impl_host,
      base::TimeTicks monotonic_time,
      bool hasUnfinishedAnimation) OVERRIDE {
    if (!num_animates_) {
      // The animation had zero duration so layerTreeHostImpl should no
      // longer need to animate its layers.
      EXPECT_FALSE(hasUnfinishedAnimation);
      num_animates_++;
      first_monotonic_time_ = monotonic_time;
      return;
    }
    EXPECT_LT(0, start_time_);
    EXPECT_TRUE(received_animation_started_notification_);
    endTest();
  }

  virtual void notifyAnimationStarted(double wall_clock_time) OVERRIDE {
    received_animation_started_notification_ = true;
    start_time_ = wall_clock_time;
  }

  virtual void afterTest() OVERRIDE {}

private:
  int num_animates_;
  bool received_animation_started_notification_;
  double start_time_;
  base::TimeTicks first_monotonic_time_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAddAnimation)

// Add a layer animation to a layer, but continually fail to draw. Confirm that
// after a while, we do eventually force a draw.
class LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws()
      : started_animating_(false) {
  }

  virtual void beginTest() OVERRIDE {
    postAddAnimationToMainThread(m_layerTreeHost->rootLayer());
  }

  virtual void animateLayers(
      LayerTreeHostImpl* host_impl,
      base::TimeTicks monotonicTime,
      bool hasUnfinishedAnimation) OVERRIDE {
    started_animating_ = true;
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl*) OVERRIDE {
    if (started_animating_)
      endTest();
  }

  virtual bool prepareToDrawOnThread(LayerTreeHostImpl*) OVERRIDE {
    return false;
  }

  virtual void afterTest() OVERRIDE {}

 private:
  bool started_animating_;
};

// Starvation can only be an issue with the MT compositor.
MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws)

// Ensures that animations continue to be ticked when we are backgrounded.
class LayerTreeHostAnimationTestTickAnimationWhileBackgrounded :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestTickAnimationWhileBackgrounded()
      : num_animates_(0) {
  }

  virtual void beginTest() OVERRIDE {
    postAddAnimationToMainThread(m_layerTreeHost->rootLayer());
  }

  // Use willAnimateLayers to set visible false before the animation runs and
  // causes a commit, so we block the second visible animate in single-thread
  // mode.
  virtual void willAnimateLayers(
      LayerTreeHostImpl* host_impl,
      base::TimeTicks monotonicTime) OVERRIDE {
    if (num_animates_ < 2) {
      if (!num_animates_) {
        // We have a long animation running. It should continue to tick even
        // if we are not visible.
        postSetVisibleToMainThread(false);
      }
      num_animates_++;
      return;
    }
    endTest();
  }

  virtual void afterTest() OVERRIDE {}

 private:
  int num_animates_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestTickAnimationWhileBackgrounded)

// Ensures that animations continue to be ticked when we are backgrounded.
class LayerTreeHostAnimationTestAddAnimationWithTimingFunction :
    public LayerTreeHostAnimationTest {
public:
  LayerTreeHostAnimationTestAddAnimationWithTimingFunction() {}

  virtual void beginTest() OVERRIDE {
    postAddAnimationToMainThread(m_layerTreeHost->rootLayer());
  }

  virtual void animateLayers(
      LayerTreeHostImpl* host_impl,
      base::TimeTicks monotonicTime,
      bool hasUnfinishedAnimation) OVERRIDE {
    LayerAnimationController* controller =
        m_layerTreeHost->rootLayer()->layerAnimationController();
    ActiveAnimation* animation =
        controller->getActiveAnimation(0, ActiveAnimation::Opacity);
    if (!animation)
      return;

    const FloatAnimationCurve* curve =
        animation->curve()->toFloatAnimationCurve();
    float startOpacity = curve->getValue(0);
    float endOpacity = curve->getValue(curve->duration());
    float linearly_interpolated_opacity =
        0.25 * endOpacity + 0.75 * startOpacity;
    double time = curve->duration() * 0.25;
    // If the linear timing function associated with this animation was not
    // picked up, then the linearly interpolated opacity would be different
    // because of the default ease timing function.
    EXPECT_FLOAT_EQ(linearly_interpolated_opacity, curve->getValue(time));

    LayerAnimationController* controller_impl =
        host_impl->rootLayer()->layerAnimationController();
    ActiveAnimation* animation_impl =
        controller_impl->getActiveAnimation(0, ActiveAnimation::Opacity);

    controller->removeAnimation(animation->id());
    controller_impl->removeAnimation(animation_impl->id());
    endTest();
  }

  virtual void afterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAddAnimationWithTimingFunction)

// Ensures that main thread animations have their start times synchronized with
// impl thread animations.
class LayerTreeHostAnimationTestSynchronizeAnimationStartTimes :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSynchronizeAnimationStartTimes()
      : main_start_time_(-1),
        impl_start_time_(-1) {
  }

  virtual void beginTest() OVERRIDE {
    postAddAnimationToMainThread(m_layerTreeHost->rootLayer());
  }

  virtual void notifyAnimationStarted(double time) OVERRIDE {
    LayerAnimationController* controller =
        m_layerTreeHost->rootLayer()->layerAnimationController();
    ActiveAnimation* animation =
        controller->getActiveAnimation(0, ActiveAnimation::Opacity);
    main_start_time_ = animation->startTime();
    controller->removeAnimation(animation->id());

    if (impl_start_time_ > 0)
      endTest();
  }

  virtual void animateLayers(
      LayerTreeHostImpl* impl_host,
      base::TimeTicks monotonicTime,
      bool hasUnfinishedAnimation) OVERRIDE {
    LayerAnimationController* controller =
        impl_host->rootLayer()->layerAnimationController();
    ActiveAnimation* animation =
        controller->getActiveAnimation(0, ActiveAnimation::Opacity);
    if (!animation)
      return;

    impl_start_time_ = animation->startTime();
    controller->removeAnimation(animation->id());

    if (main_start_time_ > 0)
      endTest();
  }

  virtual void afterTest() OVERRIDE {
    EXPECT_FLOAT_EQ(impl_start_time_, main_start_time_);
  }

 private:
  double main_start_time_;
  double impl_start_time_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestSynchronizeAnimationStartTimes)

// Ensures that notifyAnimationFinished is called.
class LayerTreeHostAnimationTestAnimationFinishedEvents :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationFinishedEvents() {}

  virtual void beginTest() OVERRIDE {
    postAddInstantAnimationToMainThread();
  }

  virtual void notifyAnimationFinished(double time) OVERRIDE {
    LayerAnimationController* controller =
        m_layerTreeHost->rootLayer()->layerAnimationController();
    ActiveAnimation* animation =
        controller->getActiveAnimation(0, ActiveAnimation::Opacity);
    controller->removeAnimation(animation->id());
    endTest();
  }

  virtual void afterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAnimationFinishedEvents)

// Ensures that when opacity is being animated, this value does not cause the
// subtree to be skipped.
class LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity()
      : update_check_layer_(FakeContentLayer::Create(&client_)) {
  }

  virtual void setupTree() OVERRIDE {
    update_check_layer_->setOpacity(0);
    m_layerTreeHost->setRootLayer(update_check_layer_);
    LayerTreeHostAnimationTest::setupTree();
  }

  virtual void beginTest() OVERRIDE {
    postAddAnimationToMainThread(update_check_layer_.get());
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE {
    endTest();
  }

  virtual void afterTest() OVERRIDE {
    // update() should have been called once, proving that the layer was not
    // skipped.
    EXPECT_EQ(1, update_check_layer_->update_count());

    // clear update_check_layer_ so LayerTreeHost dies.
    update_check_layer_ = NULL;
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> update_check_layer_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity)

// Layers added to tree with existing active animations should have the
// animation correctly recognized.
class LayerTreeHostAnimationTestLayerAddedWithAnimation :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestLayerAddedWithAnimation() { }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void didCommit() OVERRIDE {
    if (m_layerTreeHost->commitNumber() == 1) {
      scoped_refptr<Layer> layer = Layer::create();
      layer->setLayerAnimationDelegate(this);

      // Any valid AnimationCurve will do here.
      scoped_ptr<AnimationCurve> curve(EaseTimingFunction::create());
      scoped_ptr<ActiveAnimation> animation(
          ActiveAnimation::create(curve.Pass(), 1, 1,
                                  ActiveAnimation::Opacity));
      layer->layerAnimationController()->addAnimation(animation.Pass());

      // We add the animation *before* attaching the layer to the tree.
      m_layerTreeHost->rootLayer()->addChild(layer);
    }
  }

  virtual void animateLayers(
      LayerTreeHostImpl* impl_host,
      base::TimeTicks monotonic_time,
      bool hasUnfinishedAnimation) OVERRIDE {
    endTest();
  }

  virtual void afterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestLayerAddedWithAnimation)

class LayerTreeHostAnimationTestCompositeAndReadbackAnimateCount :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCompositeAndReadbackAnimateCount()
      : animated_commit_(-1) {
  }

  virtual void animate(base::TimeTicks) OVERRIDE {
    // We shouldn't animate on the compositeAndReadback-forced commit, but we
    // should for the setNeedsCommit-triggered commit.
    animated_commit_ = m_layerTreeHost->commitNumber();
    EXPECT_NE(2, animated_commit_);
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void didCommit() OVERRIDE {
    switch (m_layerTreeHost->commitNumber()) {
      case 1:
        m_layerTreeHost->setNeedsCommit();
        break;
      case 2: {
        char pixels[4];
        m_layerTreeHost->compositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
        break;
      }
      case 3:
        // This is finishing the readback's commit.
        break;
      case 4:
        // This is finishing the followup commit.
        endTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void afterTest() OVERRIDE {
    EXPECT_EQ(3, animated_commit_);
  }

 private:
  int animated_commit_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCompositeAndReadbackAnimateCount)

class LayerTreeHostAnimationTestContinuousAnimate :
    public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestContinuousAnimate()
      : num_commit_complete_(0),
        num_draw_layers_(0) {
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void animate(base::TimeTicks) OVERRIDE {
    if (num_draw_layers_ == 2)
      return;
    m_layerTreeHost->setNeedsAnimate();
  }

  virtual void layout() OVERRIDE {
    m_layerTreeHost->rootLayer()->setNeedsDisplay();
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE {
    if (num_draw_layers_ == 1)
      num_commit_complete_++;
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draw_layers_++;
    if (num_draw_layers_ == 2)
      endTest();
  }

  virtual void afterTest() OVERRIDE {
    // Check that we didn't commit twice between first and second draw.
    EXPECT_EQ(1, num_commit_complete_);
  }

 private:
  int num_commit_complete_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestContinuousAnimate)

}  // namespace
}  // namespace cc
