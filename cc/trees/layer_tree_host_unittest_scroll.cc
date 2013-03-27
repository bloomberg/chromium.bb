// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/layers/content_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerScrollClient.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {
namespace {

class LayerTreeHostScrollTest : public LayerTreeTest {};

class LayerTreeHostScrollTestScrollSimple : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollSimple()
      : initial_scroll_(10, 20),
        second_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {
  }

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->root_layer()->SetScrollable(true);
    layer_tree_host()->root_layer()->SetScrollOffset(initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void Layout() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    if (!layer_tree_host()->commit_number()) {
      EXPECT_VECTOR_EQ(initial_scroll_, root->scroll_offset());
    } else {
      EXPECT_VECTOR_EQ(initial_scroll_ + scroll_amount_, root->scroll_offset());

      // Pretend like Javascript updated the scroll position itself.
      root->SetScrollOffset(second_scroll_);
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    EXPECT_VECTOR_EQ(gfx::Vector2d(), root->scroll_delta());

    root->SetScrollable(true);
    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    root->ScrollBy(scroll_amount_);

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, root->scroll_offset());
        EXPECT_VECTOR_EQ(scroll_amount_, root->scroll_delta());
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(root->scroll_offset(), second_scroll_);
        EXPECT_VECTOR_EQ(root->scroll_delta(), scroll_amount_);
        EndTest();
        break;
    }
  }

  virtual void ApplyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
    layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1, num_scrolls_);
  }

 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d second_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollSimple);

class LayerTreeHostScrollTestScrollMultipleRedraw
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollMultipleRedraw()
      : initial_scroll_(40, 10),
        scroll_amount_(-3, 17),
        num_scrolls_(0) {
  }

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->root_layer()->SetScrollable(true);
    layer_tree_host()->root_layer()->SetScrollOffset(initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    switch (layer_tree_host()->commit_number()) {
      case 0:
        EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            root->scroll_offset(),
            initial_scroll_ + scroll_amount_ + scroll_amount_);
      case 2:
        EXPECT_VECTOR_EQ(
            root->scroll_offset(),
            initial_scroll_ + scroll_amount_ + scroll_amount_);
        break;
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    root->SetScrollable(true);
    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));

    if (impl->active_tree()->source_frame_number() == 0 &&
        impl->SourceAnimationFrameNumber() == 1) {
      // First draw after first commit.
      EXPECT_VECTOR_EQ(root->scroll_delta(), gfx::Vector2d());
      root->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(root->scroll_delta(), scroll_amount_);

      EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
      PostSetNeedsRedrawToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 0 &&
               impl->SourceAnimationFrameNumber() == 2) {
      // Second draw after first commit.
      EXPECT_EQ(root->scroll_delta(), scroll_amount_);
      root->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(root->scroll_delta(), scroll_amount_ + scroll_amount_);

      EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
      PostSetNeedsCommitToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 1) {
      // Third or later draw after second commit.
      EXPECT_GE(impl->SourceAnimationFrameNumber(), 3);
      EXPECT_VECTOR_EQ(root->scroll_delta(), gfx::Vector2d());
      EXPECT_VECTOR_EQ(
          root->scroll_offset(),
          initial_scroll_ + scroll_amount_ + scroll_amount_);
      EndTest();
    }
  }

  virtual void ApplyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
    layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1, num_scrolls_);
  }
 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollMultipleRedraw);

class LayerTreeHostScrollTestFractionalScroll : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestFractionalScroll()
      : scroll_amount_(1.75, 0) {
  }

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->root_layer()->SetScrollable(true);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));

    // Check that a fractional scroll delta is correctly accumulated over
    // multiple commits.
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(root->scroll_offset(), gfx::Vector2d(0, 0));
        EXPECT_VECTOR_EQ(root->scroll_delta(), gfx::Vector2d(0, 0));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            root->scroll_offset(),
            gfx::ToFlooredVector2d(scroll_amount_));
        EXPECT_VECTOR_EQ(
            root->scroll_delta(),
            gfx::Vector2dF(fmod(scroll_amount_.x(), 1.0f), 0.0f));
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            root->scroll_offset(),
            gfx::ToFlooredVector2d(scroll_amount_ + scroll_amount_));
        EXPECT_VECTOR_EQ(
            root->scroll_delta(),
            gfx::Vector2dF(fmod(2.0f * scroll_amount_.x(), 1.0f), 0.0f));
        EndTest();
        break;
    }
    root->ScrollBy(scroll_amount_);
  }

  virtual void ApplyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
    layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  gfx::Vector2dF scroll_amount_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestFractionalScroll);

class LayerTreeHostScrollTestCaseWithChild
    : public LayerTreeHostScrollTest,
      public WebKit::WebLayerScrollClient {
 public:
  LayerTreeHostScrollTestCaseWithChild()
      : initial_offset_(10, 20),
        javascript_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {
  }

  virtual void SetupTree() OVERRIDE {
    layer_tree_host()->SetDeviceScaleFactor(device_scale_factor_);

    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(10, 10));

    root_scroll_layer_ = ContentLayer::Create(&fake_content_layer_client_);
    root_scroll_layer_->SetBounds(gfx::Size(110, 110));

    root_scroll_layer_->SetPosition(gfx::Point(0, 0));
    root_scroll_layer_->SetAnchorPoint(gfx::PointF());

    root_scroll_layer_->SetIsDrawable(true);
    root_scroll_layer_->SetScrollable(true);
    root_scroll_layer_->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    root_layer->AddChild(root_scroll_layer_);

    child_layer_ = ContentLayer::Create(&fake_content_layer_client_);
    child_layer_->set_layer_scroll_client(this);
    child_layer_->SetBounds(gfx::Size(110, 110));

    // Scrolls on the child layer will happen at 5, 5. If they are treated
    // like device pixels, and device scale factor is 2, then they will
    // be considered at 2.5, 2.5 in logical pixels, and will miss this layer.
    child_layer_->SetPosition(gfx::Point(5, 5));
    child_layer_->SetAnchorPoint(gfx::PointF());

    child_layer_->SetIsDrawable(true);
    child_layer_->SetScrollable(true);
    child_layer_->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    root_scroll_layer_->AddChild(child_layer_);

    if (scroll_child_layer_) {
      expected_scroll_layer_ = child_layer_;
      expected_no_scroll_layer_ = root_scroll_layer_;
    } else {
      expected_scroll_layer_ = root_scroll_layer_;
      expected_no_scroll_layer_ = child_layer_;
    }

    expected_scroll_layer_->SetScrollOffset(initial_offset_);

    layer_tree_host()->SetRootLayer(root_layer);
    LayerTreeHostScrollTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void didScroll() OVERRIDE {
    final_scroll_offset_ = expected_scroll_layer_->scroll_offset();
  }

  virtual void ApplyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = root_scroll_layer_->scroll_offset();
    root_scroll_layer_->SetScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void Layout() OVERRIDE {
    EXPECT_VECTOR_EQ(
        gfx::Vector2d(), expected_no_scroll_layer_->scroll_offset());

    switch (layer_tree_host()->commit_number()) {
      case 0:
        EXPECT_VECTOR_EQ(
            initial_offset_,
            expected_scroll_layer_->scroll_offset());
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            initial_offset_ + scroll_amount_,
            expected_scroll_layer_->scroll_offset());

        // Pretend like Javascript updated the scroll position itself.
        expected_scroll_layer_->SetScrollOffset(javascript_scroll_);
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            javascript_scroll_ + scroll_amount_,
            expected_scroll_layer_->scroll_offset());
        break;
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root_impl = impl->active_tree()->root_layer();
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

    EXPECT_VECTOR_EQ(gfx::Vector2d(), root_impl->scroll_delta());
    EXPECT_VECTOR_EQ(
        gfx::Vector2d(),
        expected_no_scroll_layer_impl->scroll_delta());

    // Ensure device scale factor is affecting the layers.
    gfx::Size expected_content_bounds = gfx::ToCeiledSize(
        gfx::ScaleSize(root_scroll_layer_impl->bounds(), device_scale_factor_));
    EXPECT_SIZE_EQ(
        expected_content_bounds,
        root_scroll_layer_->content_bounds());

    expected_content_bounds = gfx::ToCeiledSize(
        gfx::ScaleSize(child_layer_impl->bounds(), device_scale_factor_));
    EXPECT_SIZE_EQ(expected_content_bounds, child_layer_->content_bounds());

    switch (impl->active_tree()->source_frame_number()) {
      case 0: {
        // Gesture scroll on impl thread.
        InputHandlerClient::ScrollStatus status = impl->ScrollBegin(
            gfx::ToCeiledPoint(
                expected_scroll_layer_impl->position() +
                gfx::Vector2dF(0.5f, 0.5f)),
            InputHandlerClient::Gesture);
        EXPECT_EQ(InputHandlerClient::ScrollStarted, status);
        impl->ScrollBy(gfx::Point(), scroll_amount_);
        impl->ScrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(
            initial_offset_,
            expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(
            scroll_amount_,
            expected_scroll_layer_impl->scroll_delta());
        break;
      }
      case 1: {
        // Wheel scroll on impl thread.
        InputHandlerClient::ScrollStatus status = impl->ScrollBegin(
            gfx::ToCeiledPoint(
                expected_scroll_layer_impl->position() +
                gfx::Vector2dF(0.5f, 0.5f)),
            InputHandlerClient::Wheel);
        EXPECT_EQ(InputHandlerClient::ScrollStarted, status);
        impl->ScrollBy(gfx::Point(), scroll_amount_);
        impl->ScrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(
            javascript_scroll_,
            expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(
            scroll_amount_,
            expected_scroll_layer_impl->scroll_delta());
        break;
      }
      case 2:

        EXPECT_VECTOR_EQ(
            javascript_scroll_ + scroll_amount_,
            expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(
            gfx::Vector2d(),
            expected_scroll_layer_impl->scroll_delta());

        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
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
  RunTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild, DeviceScaleFactor15_ScrollChild) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  RunTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild, DeviceScaleFactor2_ScrollChild) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  RunTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  RunTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  RunTest(true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(true);
}

class ImplSidePaintingScrollTest : public LayerTreeHostScrollTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // Manual vsync tick.
    if (impl->pending_tree())
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

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->root_layer()->SetScrollable(true);
    layer_tree_host()->root_layer()->SetScrollOffset(initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void Layout() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    if (!layer_tree_host()->commit_number()) {
      EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
    } else {
      EXPECT_VECTOR_EQ(root->scroll_offset(),
                       initial_scroll_ + impl_thread_scroll1_);

      // Pretend like Javascript updated the scroll position itself with a
      // change of main_thread_scroll.
      root->SetScrollOffset(initial_scroll_ +
                            main_thread_scroll_ +
                            impl_thread_scroll1_);
    }
  }

  virtual bool CanActivatePendingTree() OVERRIDE {
    return can_activate_;
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // We force a second draw here of the first commit before activating
    // the second commit.
    if (impl->active_tree()->source_frame_number() == 0)
      impl->setNeedsRedraw();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ImplSidePaintingScrollTest::DrawLayersOnThread(impl);

    LayerImpl* root = impl->active_tree()->root_layer();
    root->SetScrollable(true);
    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));

    LayerImpl* pending_root =
        impl->active_tree()->FindPendingTreeLayerById(root->id());

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        if (!impl->pending_tree()) {
          can_activate_ = false;
          EXPECT_VECTOR_EQ(root->scroll_delta(), gfx::Vector2d());
          root->ScrollBy(impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
          EXPECT_VECTOR_EQ(root->scroll_delta(), impl_thread_scroll1_);
          EXPECT_VECTOR_EQ(root->sent_scroll_delta(), gfx::Vector2d());
          PostSetNeedsCommitToMainThread();

          // CommitCompleteOnThread will trigger this function again
          // and cause us to take the else clause.
        } else {
          can_activate_ = true;
          ASSERT_TRUE(pending_root);
          EXPECT_EQ(impl->pending_tree()->source_frame_number(), 1);

          root->ScrollBy(impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
          EXPECT_VECTOR_EQ(root->scroll_delta(),
              impl_thread_scroll1_ + impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(root->sent_scroll_delta(), impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(pending_root->scroll_offset(),
              initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
          EXPECT_VECTOR_EQ(pending_root->scroll_delta(), impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(pending_root->sent_scroll_delta(), gfx::Vector2d());
        }
        break;
      case 1:
        EXPECT_FALSE(impl->pending_tree());
        EXPECT_VECTOR_EQ(root->scroll_offset(),
            initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
        EXPECT_VECTOR_EQ(root->scroll_delta(), impl_thread_scroll2_);
        EXPECT_VECTOR_EQ(root->sent_scroll_delta(), gfx::Vector2d());
        EndTest();
        break;
    }
  }

  virtual void ApplyScrollAndScale(
      gfx::Vector2d scroll_delta, float scale) OVERRIDE {
    gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
    layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE {
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

class LayerTreeHostScrollTestScrollZeroMaxScrollOffset
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollZeroMaxScrollOffset() {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    root->SetScrollable(true);

    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    EXPECT_EQ(
        InputHandlerClient::ScrollStarted,
        root->TryScroll(
            gfx::PointF(0.0f, 1.0f),
            InputHandlerClient::Gesture));

    root->SetMaxScrollOffset(gfx::Vector2d(0, 0));
    EXPECT_EQ(
        InputHandlerClient::ScrollIgnored,
        root->TryScroll(
            gfx::PointF(0.0f, 1.0f),
            InputHandlerClient::Gesture));

    root->SetMaxScrollOffset(gfx::Vector2d(-100, -100));
    EXPECT_EQ(
        InputHandlerClient::ScrollIgnored,
        root->TryScroll(
            gfx::PointF(0.0f, 1.0f),
            InputHandlerClient::Gesture));

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostScrollTestScrollZeroMaxScrollOffset);

}  // namespace
}  // namespace cc
