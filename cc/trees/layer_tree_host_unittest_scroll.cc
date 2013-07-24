// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/memory/weak_ptr.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"
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
        num_scrolls_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->root_layer()->SetScrollable(true);
    layer_tree_host()->root_layer()->SetScrollOffset(initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void Layout() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    if (!layer_tree_host()->source_frame_number()) {
      EXPECT_VECTOR_EQ(initial_scroll_, root->scroll_offset());
    } else {
      EXPECT_VECTOR_EQ(initial_scroll_ + scroll_amount_, root->scroll_offset());

      // Pretend like Javascript updated the scroll position itself.
      root->SetScrollOffset(second_scroll_);
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    EXPECT_VECTOR_EQ(gfx::Vector2d(), root->ScrollDelta());

    root->SetScrollable(true);
    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    root->ScrollBy(scroll_amount_);

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, root->scroll_offset());
        EXPECT_VECTOR_EQ(scroll_amount_, root->ScrollDelta());
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(root->scroll_offset(), second_scroll_);
        EXPECT_VECTOR_EQ(root->ScrollDelta(), scroll_amount_);
        EndTest();
        break;
    }
  }

  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta, float scale)
      OVERRIDE {
    gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
    layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_scrolls_); }

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
      : initial_scroll_(40, 10), scroll_amount_(-3, 17), num_scrolls_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->root_layer()->SetScrollable(true);
    layer_tree_host()->root_layer()->SetScrollOffset(initial_scroll_);
    layer_tree_host()->root_layer()->SetBounds(gfx::Size(200, 200));
    PostSetNeedsCommitToMainThread();
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
        break;
      case 1:
        EXPECT_VECTOR_EQ(root->scroll_offset(),
                         initial_scroll_ + scroll_amount_ + scroll_amount_);
      case 2:
        EXPECT_VECTOR_EQ(root->scroll_offset(),
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
      EXPECT_VECTOR_EQ(root->ScrollDelta(), gfx::Vector2d());
      root->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(root->ScrollDelta(), scroll_amount_);

      EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
      PostSetNeedsRedrawToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 0 &&
               impl->SourceAnimationFrameNumber() == 2) {
      // Second draw after first commit.
      EXPECT_EQ(root->ScrollDelta(), scroll_amount_);
      root->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(root->ScrollDelta(), scroll_amount_ + scroll_amount_);

      EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
      PostSetNeedsCommitToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 1) {
      // Third or later draw after second commit.
      EXPECT_GE(impl->SourceAnimationFrameNumber(), 3);
      EXPECT_VECTOR_EQ(root->ScrollDelta(), gfx::Vector2d());
      EXPECT_VECTOR_EQ(root->scroll_offset(),
                       initial_scroll_ + scroll_amount_ + scroll_amount_);
      EndTest();
    }
  }

  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta, float scale)
      OVERRIDE {
    gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
    layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_scrolls_); }

 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollMultipleRedraw);

class LayerTreeHostScrollTestFractionalScroll : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestFractionalScroll() : scroll_amount_(1.75, 0) {}

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
        EXPECT_VECTOR_EQ(root->ScrollDelta(), gfx::Vector2d(0, 0));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(root->scroll_offset(),
                         gfx::ToFlooredVector2d(scroll_amount_));
        EXPECT_VECTOR_EQ(root->ScrollDelta(),
                         gfx::Vector2dF(fmod(scroll_amount_.x(), 1.0f), 0.0f));
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            root->scroll_offset(),
            gfx::ToFlooredVector2d(scroll_amount_ + scroll_amount_));
        EXPECT_VECTOR_EQ(
            root->ScrollDelta(),
            gfx::Vector2dF(fmod(2.0f * scroll_amount_.x(), 1.0f), 0.0f));
        EndTest();
        break;
    }
    root->ScrollBy(scroll_amount_);
  }

  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta, float scale)
      OVERRIDE {
    gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
    layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  gfx::Vector2dF scroll_amount_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestFractionalScroll);

class LayerTreeHostScrollTestCaseWithChild : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestCaseWithChild()
      : initial_offset_(10, 20),
        javascript_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {}

  virtual void SetupTree() OVERRIDE {
    layer_tree_host()->SetDeviceScaleFactor(device_scale_factor_);

    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(10, 10));

    root_scroll_layer_ = ContentLayer::Create(&fake_content_layer_client_);
    root_scroll_layer_->SetBounds(gfx::Size(110, 110));

    root_scroll_layer_->SetPosition(gfx::Point());
    root_scroll_layer_->SetAnchorPoint(gfx::PointF());

    root_scroll_layer_->SetIsDrawable(true);
    root_scroll_layer_->SetScrollable(true);
    root_scroll_layer_->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    root_layer->AddChild(root_scroll_layer_);

    child_layer_ = ContentLayer::Create(&fake_content_layer_client_);
    child_layer_->set_did_scroll_callback(
        base::Bind(&LayerTreeHostScrollTestCaseWithChild::DidScroll,
                   base::Unretained(this)));
    child_layer_->SetBounds(gfx::Size(110, 110));

    if (scroll_child_layer_) {
      // Scrolls on the child layer will happen at 5, 5. If they are treated
      // like device pixels, and device scale factor is 2, then they will
      // be considered at 2.5, 2.5 in logical pixels, and will miss this layer.
      child_layer_->SetPosition(gfx::Point(5, 5));
    } else {
      // Adjust the child layer horizontally so that scrolls will never hit it.
      child_layer_->SetPosition(gfx::Point(60, 5));
    }
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

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  void DidScroll() {
    final_scroll_offset_ = expected_scroll_layer_->scroll_offset();
  }

  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta, float scale)
      OVERRIDE {
    gfx::Vector2d offset = root_scroll_layer_->scroll_offset();
    root_scroll_layer_->SetScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void Layout() OVERRIDE {
    EXPECT_VECTOR_EQ(gfx::Vector2d(),
                     expected_no_scroll_layer_->scroll_offset());

    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_offset_,
                         expected_scroll_layer_->scroll_offset());
        break;
      case 1:
        EXPECT_VECTOR_EQ(initial_offset_ + scroll_amount_,
                         expected_scroll_layer_->scroll_offset());

        // Pretend like Javascript updated the scroll position itself.
        expected_scroll_layer_->SetScrollOffset(javascript_scroll_);
        break;
      case 2:
        EXPECT_VECTOR_EQ(javascript_scroll_ + scroll_amount_,
                         expected_scroll_layer_->scroll_offset());
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
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

    EXPECT_VECTOR_EQ(gfx::Vector2d(), root_impl->ScrollDelta());
    EXPECT_VECTOR_EQ(gfx::Vector2d(),
                     expected_no_scroll_layer_impl->ScrollDelta());

    // Ensure device scale factor is affecting the layers.
    gfx::Size expected_content_bounds = gfx::ToCeiledSize(
        gfx::ScaleSize(root_scroll_layer_impl->bounds(), device_scale_factor_));
    EXPECT_SIZE_EQ(expected_content_bounds,
                   root_scroll_layer_->content_bounds());

    expected_content_bounds = gfx::ToCeiledSize(
        gfx::ScaleSize(child_layer_impl->bounds(), device_scale_factor_));
    EXPECT_SIZE_EQ(expected_content_bounds, child_layer_->content_bounds());

    switch (impl->active_tree()->source_frame_number()) {
      case 0: {
        // Gesture scroll on impl thread.
        InputHandler::ScrollStatus status = impl->ScrollBegin(
            gfx::ToCeiledPoint(expected_scroll_layer_impl->position() -
                               gfx::Vector2dF(0.5f, 0.5f)),
            InputHandler::Gesture);
        EXPECT_EQ(InputHandler::ScrollStarted, status);
        impl->ScrollBy(gfx::Point(), scroll_amount_);
        impl->ScrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(initial_offset_,
                         expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(scroll_amount_,
                         expected_scroll_layer_impl->ScrollDelta());
        break;
      }
      case 1: {
        // Wheel scroll on impl thread.
        InputHandler::ScrollStatus status = impl->ScrollBegin(
            gfx::ToCeiledPoint(expected_scroll_layer_impl->position() +
                               gfx::Vector2dF(0.5f, 0.5f)),
            InputHandler::Wheel);
        EXPECT_EQ(InputHandler::ScrollStarted, status);
        impl->ScrollBy(gfx::Point(), scroll_amount_);
        impl->ScrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(javascript_scroll_,
                         expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(scroll_amount_,
                         expected_scroll_layer_impl->ScrollDelta());
        break;
      }
      case 2:

        EXPECT_VECTOR_EQ(javascript_scroll_ + scroll_amount_,
                         expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(gfx::Vector2d(),
                         expected_scroll_layer_impl->ScrollDelta());

        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    if (scroll_child_layer_) {
      EXPECT_EQ(0, num_scrolls_);
      EXPECT_VECTOR_EQ(javascript_scroll_ + scroll_amount_,
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

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DirectRenderer_MainThreadPaint) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DirectRenderer_ImplSidePaint) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DelegatingRenderer_MainThreadPaint) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(true, true, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DelegatingRenderer_ImplSidePaint) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollChild_DirectRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollChild_DelegatingRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollChild_DirectRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollChild_DelegatingRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer_DirectRenderer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer_DirectRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer_DirectRenderer_MainSidePaint) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer_DirectRenderer_ImplSidePaint) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(true, true, true);
}

class ImplSidePaintingScrollTest : public LayerTreeHostScrollTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (impl->pending_tree())
      impl->SetNeedsRedraw();
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
        can_activate_(true) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->root_layer()->SetScrollable(true);
    layer_tree_host()->root_layer()->SetScrollOffset(initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void Layout() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    if (!layer_tree_host()->source_frame_number()) {
      EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
    } else {
      EXPECT_VECTOR_EQ(root->scroll_offset(),
                       initial_scroll_ + impl_thread_scroll1_);

      // Pretend like Javascript updated the scroll position itself with a
      // change of main_thread_scroll.
      root->SetScrollOffset(initial_scroll_ + main_thread_scroll_ +
                            impl_thread_scroll1_);
    }
  }

  virtual bool CanActivatePendingTree(LayerTreeHostImpl* impl) OVERRIDE {
    return can_activate_;
  }

  virtual bool CanActivatePendingTreeIfNeeded(LayerTreeHostImpl* impl)
      OVERRIDE {
    return can_activate_;
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // We force a second draw here of the first commit before activating
    // the second commit.
    if (impl->active_tree()->source_frame_number() == 0)
      impl->SetNeedsRedraw();
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
          EXPECT_VECTOR_EQ(root->ScrollDelta(), gfx::Vector2d());
          root->ScrollBy(impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(root->scroll_offset(), initial_scroll_);
          EXPECT_VECTOR_EQ(root->ScrollDelta(), impl_thread_scroll1_);
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
          EXPECT_VECTOR_EQ(root->ScrollDelta(),
                           impl_thread_scroll1_ + impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(root->sent_scroll_delta(), impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(
              pending_root->scroll_offset(),
              initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
          EXPECT_VECTOR_EQ(pending_root->ScrollDelta(), impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(pending_root->sent_scroll_delta(), gfx::Vector2d());
        }
        break;
      case 1:
        EXPECT_FALSE(impl->pending_tree());
        EXPECT_VECTOR_EQ(
            root->scroll_offset(),
            initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
        EXPECT_VECTOR_EQ(root->ScrollDelta(), impl_thread_scroll2_);
        EXPECT_VECTOR_EQ(root->sent_scroll_delta(), gfx::Vector2d());
        EndTest();
        break;
    }
  }

  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta, float scale)
      OVERRIDE {
    gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
    layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_scrolls_); }

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

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    root->SetScrollable(true);

    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    EXPECT_EQ(InputHandler::ScrollStarted,
              root->TryScroll(gfx::PointF(0.0f, 1.0f), InputHandler::Gesture));

    root->SetMaxScrollOffset(gfx::Vector2d(0, 0));
    EXPECT_EQ(InputHandler::ScrollIgnored,
              root->TryScroll(gfx::PointF(0.0f, 1.0f), InputHandler::Gesture));

    root->SetMaxScrollOffset(gfx::Vector2d(-100, -100));
    EXPECT_EQ(InputHandler::ScrollIgnored,
              root->TryScroll(gfx::PointF(0.0f, 1.0f), InputHandler::Gesture));

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostScrollTestScrollZeroMaxScrollOffset);

class ThreadCheckingInputHandlerClient : public InputHandlerClient {
 public:
  ThreadCheckingInputHandlerClient(base::SingleThreadTaskRunner* runner,
                                   bool* received_stop_flinging)
      : task_runner_(runner), received_stop_flinging_(received_stop_flinging) {}

  virtual void WillShutdown() OVERRIDE {
    if (!received_stop_flinging_)
      ADD_FAILURE() << "WillShutdown() called before fling stopped";
  }

  virtual void Animate(base::TimeTicks time) OVERRIDE {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "Animate called on wrong thread";
  }

  virtual void MainThreadHasStoppedFlinging() OVERRIDE {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "MainThreadHasStoppedFlinging called on wrong thread";
    *received_stop_flinging_ = true;
  }

  virtual void DidOverscroll(const DidOverscrollParams& params) OVERRIDE {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "DidOverscroll called on wrong thread";
  }

 private:
  base::SingleThreadTaskRunner* task_runner_;
  bool* received_stop_flinging_;
};

void BindInputHandlerOnCompositorThread(
    const base::WeakPtr<InputHandler>& input_handler,
    ThreadCheckingInputHandlerClient* client) {
  input_handler->BindToClient(client);
}

TEST(LayerTreeHostFlingTest, DidStopFlingingThread) {
  base::Thread impl_thread("cc");
  ASSERT_TRUE(impl_thread.Start());

  bool received_stop_flinging = false;
  LayerTreeSettings settings;

  ThreadCheckingInputHandlerClient input_handler_client(
          impl_thread.message_loop_proxy().get(), &received_stop_flinging);
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

  ASSERT_TRUE(impl_thread.message_loop_proxy().get());
  scoped_ptr<LayerTreeHost> layer_tree_host = LayerTreeHost::Create(
      &client, settings, impl_thread.message_loop_proxy());

  impl_thread.message_loop_proxy()
      ->PostTask(FROM_HERE,
                 base::Bind(&BindInputHandlerOnCompositorThread,
                            layer_tree_host->GetInputHandler(),
                            base::Unretained(&input_handler_client)));

  layer_tree_host->DidStopFlinging();
  layer_tree_host.reset();
  impl_thread.Stop();
  EXPECT_TRUE(received_stop_flinging);
}

class LayerTreeHostScrollTestLayerStructureChange
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestLayerStructureChange()
      : scroll_destroy_whole_tree_(false) {}

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(10, 10));

    Layer* root_scroll_layer =
        CreateScrollLayer(root_layer.get(), &root_scroll_layer_client_);
    CreateScrollLayer(root_layer.get(), &sibling_scroll_layer_client_);
    CreateScrollLayer(root_scroll_layer, &child_scroll_layer_client_);

    layer_tree_host()->SetRootLayer(root_layer);
    LayerTreeHostScrollTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        root->child_at(0)->SetScrollDelta(gfx::Vector2dF(5, 5));
        root->child_at(0)->child_at(0)->SetScrollDelta(gfx::Vector2dF(5, 5));
        root->child_at(1)->SetScrollDelta(gfx::Vector2dF(5, 5));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void DidScroll(Layer* layer) {
    if (scroll_destroy_whole_tree_) {
      layer_tree_host()->SetRootLayer(NULL);
      EndTest();
      return;
    }
    layer->RemoveFromParent();
  }

 protected:
  class FakeLayerScrollClient {
   public:
    void DidScroll() {
      owner_->DidScroll(layer_);
    }
    LayerTreeHostScrollTestLayerStructureChange* owner_;
    Layer* layer_;
  };

  Layer* CreateScrollLayer(Layer* parent, FakeLayerScrollClient* client) {
    scoped_refptr<Layer> scroll_layer =
        ContentLayer::Create(&fake_content_layer_client_);
    scroll_layer->SetBounds(gfx::Size(110, 110));
    scroll_layer->SetPosition(gfx::Point(0, 0));
    scroll_layer->SetAnchorPoint(gfx::PointF());
    scroll_layer->SetIsDrawable(true);
    scroll_layer->SetScrollable(true);
    scroll_layer->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    scroll_layer->set_did_scroll_callback(base::Bind(
        &FakeLayerScrollClient::DidScroll, base::Unretained(client)));
    client->owner_ = this;
    client->layer_ = scroll_layer.get();
    parent->AddChild(scroll_layer);
    return scroll_layer.get();
  }

  FakeLayerScrollClient root_scroll_layer_client_;
  FakeLayerScrollClient sibling_scroll_layer_client_;
  FakeLayerScrollClient child_scroll_layer_client_;

  FakeContentLayerClient fake_content_layer_client_;

  bool scroll_destroy_whole_tree_;
};

TEST_F(LayerTreeHostScrollTestLayerStructureChange, ScrollDestroyLayer) {
    RunTest(true, false, false);
}

TEST_F(LayerTreeHostScrollTestLayerStructureChange, ScrollDestroyWholeTree) {
    scroll_destroy_whole_tree_ = true;
    RunTest(true, false, false);
}

}  // namespace
}  // namespace cc
