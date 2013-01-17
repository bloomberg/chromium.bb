// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "cc/content_layer.h"
#include "cc/layer.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_impl.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test_common.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerScrollClient.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {
namespace {

class LayerTreeHostScrollTest : public ThreadedTest {};

class LayerTreeHostScrollTestScrollSimple : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollSimple()
      : initial_scroll_(10, 20),
        second_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {
  }

  virtual void beginTest() OVERRIDE {
    m_layerTreeHost->rootLayer()->setScrollable(true);
    m_layerTreeHost->rootLayer()->setScrollOffset(initial_scroll_);
    postSetNeedsCommitToMainThread();
  }

  virtual void layout() OVERRIDE {
    Layer* root = m_layerTreeHost->rootLayer();
    if (!m_layerTreeHost->commitNumber())
      EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_);
    else {
      EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_ + scroll_amount_);

      // Pretend like Javascript updated the scroll position itself.
      root->setScrollOffset(second_scroll_);
    }
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->rootLayer();
    EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());

    root->setScrollable(true);
    root->setMaxScrollOffset(gfx::Vector2d(100, 100));
    root->scrollBy(scroll_amount_);

    switch (impl->activeTree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_);
        EXPECT_VECTOR_EQ(root->scrollDelta(), scroll_amount_);
        postSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(root->scrollOffset(), second_scroll_);
        EXPECT_VECTOR_EQ(root->scrollDelta(), scroll_amount_);
        endTest();
        break;
    }
  }

  virtual void applyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
    m_layerTreeHost->rootLayer()->setScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void afterTest() OVERRIDE {
    EXPECT_EQ(1, num_scrolls_);
  }

 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d second_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollSimple)

class LayerTreeHostScrollTestScrollMultipleRedraw :
    public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollMultipleRedraw()
      : initial_scroll_(40, 10),
        scroll_amount_(-3, 17),
        num_scrolls_(0) {
  }

  virtual void beginTest() OVERRIDE {
    m_layerTreeHost->rootLayer()->setScrollable(true);
    m_layerTreeHost->rootLayer()->setScrollOffset(initial_scroll_);
    postSetNeedsCommitToMainThread();
  }

  virtual void beginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    Layer* root = m_layerTreeHost->rootLayer();
    switch (m_layerTreeHost->commitNumber()) {
      case 0:
        EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_);
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            root->scrollOffset(),
            initial_scroll_ + scroll_amount_ + scroll_amount_);
      case 2:
        EXPECT_VECTOR_EQ(
            root->scrollOffset(),
            initial_scroll_ + scroll_amount_ + scroll_amount_);
        break;
    }
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->rootLayer();
    root->setScrollable(true);
    root->setMaxScrollOffset(gfx::Vector2d(100, 100));

    if (impl->activeTree()->source_frame_number() == 0 &&
        impl->sourceAnimationFrameNumber() == 1) {
      // First draw after first commit.
      EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());
      root->scrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(root->scrollDelta(), scroll_amount_);

      EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_);
      postSetNeedsRedrawToMainThread();
    } else if (impl->activeTree()->source_frame_number() == 0 &&
               impl->sourceAnimationFrameNumber() == 2) {
      // Second draw after first commit.
      EXPECT_EQ(root->scrollDelta(), scroll_amount_);
      root->scrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(root->scrollDelta(), scroll_amount_ + scroll_amount_);

      EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_);
      postSetNeedsCommitToMainThread();
    } else if (impl->activeTree()->source_frame_number() == 1) {
      // Third or later draw after second commit.
      EXPECT_GE(impl->sourceAnimationFrameNumber(), 3);
      EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());
      EXPECT_VECTOR_EQ(
          root->scrollOffset(),
          initial_scroll_ + scroll_amount_ + scroll_amount_);
      endTest();
    }
  }

  virtual void applyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
    m_layerTreeHost->rootLayer()->setScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void afterTest() OVERRIDE {
    EXPECT_EQ(1, num_scrolls_);
  }
 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollMultipleRedraw)

class LayerTreeHostScrollTestFractionalScroll : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestFractionalScroll()
      : scroll_amount_(1.75, 0) {
  }

  virtual void beginTest() OVERRIDE {
    m_layerTreeHost->rootLayer()->setScrollable(true);
    postSetNeedsCommitToMainThread();
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->rootLayer();
    root->setMaxScrollOffset(gfx::Vector2d(100, 100));

    // Check that a fractional scroll delta is correctly accumulated over
    // multiple commits.
    switch (impl->activeTree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(root->scrollOffset(), gfx::Vector2d(0, 0));
        EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d(0, 0));
        postSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            root->scrollOffset(),
            gfx::ToFlooredVector2d(scroll_amount_));
        EXPECT_VECTOR_EQ(
            root->scrollDelta(),
            gfx::Vector2dF(fmod(scroll_amount_.x(), 1.0f), 0.0f));
        postSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            root->scrollOffset(),
            gfx::ToFlooredVector2d(scroll_amount_ + scroll_amount_));
        EXPECT_VECTOR_EQ(
            root->scrollDelta(),
            gfx::Vector2dF(fmod(2.0f * scroll_amount_.x(), 1.0f), 0.0f));
        endTest();
        break;
    }
    root->scrollBy(scroll_amount_);
  }

  virtual void applyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
    m_layerTreeHost->rootLayer()->setScrollOffset(offset + scroll_delta);
  }

  virtual void afterTest() OVERRIDE {}

 private:
  gfx::Vector2dF scroll_amount_;
};

TEST_F(LayerTreeHostScrollTestFractionalScroll, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostScrollTestCaseWithChild :
    public LayerTreeHostScrollTest,
    public WebKit::WebLayerScrollClient {
 public:
  LayerTreeHostScrollTestCaseWithChild()
      : initial_offset_(10, 20),
        javascript_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {
  }

  virtual void setupTree() OVERRIDE {
    m_layerTreeHost->setDeviceScaleFactor(device_scale_factor_);

    scoped_refptr<Layer> root_layer = Layer::create();
    root_layer->setBounds(gfx::Size(10, 10));

    root_scroll_layer_ = ContentLayer::create(&fake_content_layer_client_);
    root_scroll_layer_->setBounds(gfx::Size(110, 110));

    root_scroll_layer_->setPosition(gfx::Point(0, 0));
    root_scroll_layer_->setAnchorPoint(gfx::PointF());

    root_scroll_layer_->setIsDrawable(true);
    root_scroll_layer_->setScrollable(true);
    root_scroll_layer_->setMaxScrollOffset(gfx::Vector2d(100, 100));
    root_layer->addChild(root_scroll_layer_);

    child_layer_ = ContentLayer::create(&fake_content_layer_client_);
    child_layer_->setLayerScrollClient(this);
    child_layer_->setBounds(gfx::Size(110, 110));

    // Scrolls on the child layer will happen at 5, 5. If they are treated
    // like device pixels, and device scale factor is 2, then they will
    // be considered at 2.5, 2.5 in logical pixels, and will miss this layer.
    child_layer_->setPosition(gfx::Point(5, 5));
    child_layer_->setAnchorPoint(gfx::PointF());

    child_layer_->setIsDrawable(true);
    child_layer_->setScrollable(true);
    child_layer_->setMaxScrollOffset(gfx::Vector2d(100, 100));
    root_scroll_layer_->addChild(child_layer_);

    if (scroll_child_layer_) {
      expected_scroll_layer_ = child_layer_;
      expected_no_scroll_layer_ = root_scroll_layer_;
    } else {
      expected_scroll_layer_ = root_scroll_layer_;
      expected_no_scroll_layer_ = child_layer_;
    }

    expected_scroll_layer_->setScrollOffset(initial_offset_);

    m_layerTreeHost->setRootLayer(root_layer);
    LayerTreeHostScrollTest::setupTree();
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void didScroll() OVERRIDE {
    final_scroll_offset_ = expected_scroll_layer_->scrollOffset();
  }

  virtual void applyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = root_scroll_layer_->scrollOffset();
    root_scroll_layer_->setScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void layout() OVERRIDE {
    EXPECT_VECTOR_EQ(
        gfx::Vector2d(), expected_no_scroll_layer_->scrollOffset());

    switch (m_layerTreeHost->commitNumber()) {
      case 0:
        EXPECT_VECTOR_EQ(
            initial_offset_,
            expected_scroll_layer_->scrollOffset());
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            initial_offset_ + scroll_amount_,
            expected_scroll_layer_->scrollOffset());

        // Pretend like Javascript updated the scroll position itself.
        expected_scroll_layer_->setScrollOffset(javascript_scroll_);
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            javascript_scroll_ + scroll_amount_,
            expected_scroll_layer_->scrollOffset());
        break;
    }
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root_impl = impl->rootLayer();
    LayerImpl* root_scroll_layer_impl = root_impl->children()[0];
    LayerImpl* child_layer_impl = root_scroll_layer_impl->children()[0];

    LayerImpl* expected_scroll_layer_impl = NULL;
    LayerImpl* expected_no_scroll_layer_impl = NULL;
    if (scroll_child_layer_) {
      expected_scroll_layer_impl = child_layer_impl;
      expected_no_scroll_layer_impl = root_scroll_layer_impl;
    } else {
      expected_scroll_layer_impl = root_scroll_layer_impl;
      expected_no_scroll_layer_impl = child_layer_impl;
    }

    EXPECT_VECTOR_EQ(gfx::Vector2d(), root_impl->scrollDelta());
    EXPECT_VECTOR_EQ(
        gfx::Vector2d(),
        expected_no_scroll_layer_impl->scrollDelta());

    // Ensure device scale factor is affecting the layers.
    gfx::Size expected_content_bounds = gfx::ToCeiledSize(
        gfx::ScaleSize(root_scroll_layer_impl->bounds(), device_scale_factor_));
    EXPECT_SIZE_EQ(
        expected_content_bounds,
        root_scroll_layer_->contentBounds());

    expected_content_bounds = gfx::ToCeiledSize(
        gfx::ScaleSize(child_layer_impl->bounds(), device_scale_factor_));
    EXPECT_SIZE_EQ(expected_content_bounds, child_layer_->contentBounds());

    switch (impl->activeTree()->source_frame_number()) {
      case 0: {
        // Gesture scroll on impl thread.
        InputHandlerClient::ScrollStatus status = impl->scrollBegin(
            gfx::ToCeiledPoint(
                expected_scroll_layer_impl->position() +
                gfx::Vector2dF(0.5f, 0.5f)),
            InputHandlerClient::Gesture);
        EXPECT_EQ(InputHandlerClient::ScrollStarted, status);
        impl->scrollBy(gfx::Point(), scroll_amount_);
        impl->scrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(
            initial_offset_,
            expected_scroll_layer_impl->scrollOffset());
        EXPECT_VECTOR_EQ(
            scroll_amount_,
            expected_scroll_layer_impl->scrollDelta());
        break;
      }
      case 1: {
        // Wheel scroll on impl thread.
        InputHandlerClient::ScrollStatus status = impl->scrollBegin(
            gfx::ToCeiledPoint(
                expected_scroll_layer_impl->position() +
                gfx::Vector2dF(0.5f, 0.5f)),
            InputHandlerClient::Wheel);
        EXPECT_EQ(InputHandlerClient::ScrollStarted, status);
        impl->scrollBy(gfx::Point(), scroll_amount_);
        impl->scrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(
            javascript_scroll_,
            expected_scroll_layer_impl->scrollOffset());
        EXPECT_VECTOR_EQ(
            scroll_amount_,
            expected_scroll_layer_impl->scrollDelta());
        break;
      }
      case 2:

        EXPECT_VECTOR_EQ(
            javascript_scroll_ + scroll_amount_,
            expected_scroll_layer_impl->scrollOffset());
        EXPECT_VECTOR_EQ(
            gfx::Vector2d(),
            expected_scroll_layer_impl->scrollDelta());

        endTest();
        break;
    }
  }

  virtual void afterTest() OVERRIDE {
    if (scroll_child_layer_) {
      EXPECT_EQ(0, num_scrolls_);
      EXPECT_VECTOR_EQ(
          javascript_scroll_ + scroll_amount_,
          final_scroll_offset_);
    } else {
      EXPECT_EQ(2, num_scrolls_);
      EXPECT_VECTOR_EQ(gfx::Vector2d(), final_scroll_offset_);
    }
  }

 protected:
  float device_scale_factor_;
  bool scroll_child_layer_;

  gfx::Vector2d initial_offset_;
  gfx::Vector2d javascript_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
  gfx::Vector2d final_scroll_offset_;

  FakeContentLayerClient fake_content_layer_client_;

  scoped_refptr<Layer> root_scroll_layer_;
  scoped_refptr<Layer> child_layer_;
  scoped_refptr<Layer> expected_scroll_layer_;
  scoped_refptr<Layer> expected_no_scroll_layer_;
};

TEST_F(LayerTreeHostScrollTestCaseWithChild, DeviceScaleFactor1_ScrollChild) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  runTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild, DeviceScaleFactor15_ScrollChild) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  runTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild, DeviceScaleFactor2_ScrollChild) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  runTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  runTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  runTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  runTest(true);
}

class ImplSidePaintingScrollTest : public LayerTreeHostScrollTest {
 public:
  virtual void initializeSettings(LayerTreeSettings& settings) OVERRIDE {
    settings.implSidePainting = true;
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // Manual vsync tick.
    if (impl->pendingTree())
      impl->setNeedsRedraw();
  }
};

class ImplSidePaintingScrollTestSimple : public ImplSidePaintingScrollTest {
 public:
  ImplSidePaintingScrollTestSimple()
      : initial_scroll_(10, 20),
        main_thread_scroll_(40, 5),
        impl_thread_scroll1_(2, -1),
        impl_thread_scroll2_(-3, 10),
        num_scrolls_(0),
        can_activate_(true) {
  }

  virtual void beginTest() OVERRIDE {
    m_layerTreeHost->rootLayer()->setScrollable(true);
    m_layerTreeHost->rootLayer()->setScrollOffset(initial_scroll_);
    postSetNeedsCommitToMainThread();
  }

  virtual void layout() OVERRIDE {
    Layer* root = m_layerTreeHost->rootLayer();
    if (!m_layerTreeHost->commitNumber())
      EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_);
    else {
      EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_ + impl_thread_scroll1_);

      // Pretend like Javascript updated the scroll position itself with a
      // change of main_thread_scroll.
      root->setScrollOffset(initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
    }
  }

  virtual bool canActivatePendingTree() OVERRIDE {
    return can_activate_;
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // We force a second draw here of the first commit before activating
    // the second commit.
    if (impl->activeTree()->source_frame_number() == 0)
      impl->setNeedsRedraw();
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ImplSidePaintingScrollTest::drawLayersOnThread(impl);

    LayerImpl* root = impl->rootLayer();
    root->setScrollable(true);
    root->setMaxScrollOffset(gfx::Vector2d(100, 100));

    LayerImpl* pending_root =
        impl->activeTree()->FindPendingTreeLayerById(root->id());

    switch (impl->activeTree()->source_frame_number()) {
      case 0:
        if (!impl->pendingTree()) {
          can_activate_ = false;
          EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());
          root->scrollBy(impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_);
          EXPECT_VECTOR_EQ(root->scrollDelta(), impl_thread_scroll1_);
          EXPECT_VECTOR_EQ(root->sentScrollDelta(), gfx::Vector2d());
          postSetNeedsCommitToMainThread();

          // commitCompleteOnThread will trigger this function again
          // and cause us to take the else clause.
        } else {
          can_activate_ = true;
          ASSERT_TRUE(pending_root);
          EXPECT_EQ(impl->pendingTree()->source_frame_number(), 1);

          root->scrollBy(impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(root->scrollOffset(), initial_scroll_);
          EXPECT_VECTOR_EQ(root->scrollDelta(),
              impl_thread_scroll1_ + impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(root->sentScrollDelta(), impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(pending_root->scrollOffset(),
              initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
          EXPECT_VECTOR_EQ(pending_root->scrollDelta(), impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(pending_root->sentScrollDelta(), gfx::Vector2d());
        }
        break;
      case 1:
        EXPECT_FALSE(impl->pendingTree());
        EXPECT_VECTOR_EQ(root->scrollOffset(),
            initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
        EXPECT_VECTOR_EQ(root->scrollDelta(), impl_thread_scroll2_);
        EXPECT_VECTOR_EQ(root->sentScrollDelta(), gfx::Vector2d());
        endTest();
        break;
    }
  }

  virtual void applyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
    m_layerTreeHost->rootLayer()->setScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void afterTest() OVERRIDE {
    EXPECT_EQ(1, num_scrolls_);
  }

 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d main_thread_scroll_;
  gfx::Vector2d impl_thread_scroll1_;
  gfx::Vector2d impl_thread_scroll2_;
  int num_scrolls_;
  bool can_activate_;
};

MULTI_THREAD_TEST_F(ImplSidePaintingScrollTestSimple);

}  // namespace
}  // namespace cc
