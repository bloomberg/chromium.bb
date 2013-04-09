// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

// These tests deal with damage tracking.
class LayerTreeHostDamageTest : public LayerTreeTest {};

class LayerTreeHostDamageTestNoDamageDoesNotSwap
    : public LayerTreeHostDamageTest {
  virtual void BeginTest() OVERRIDE {
    expect_swap_and_succeed_ = 0;
    did_swaps_ = 0;
    did_swap_and_succeed_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<FakeContentLayer> root = FakeContentLayer::Create(&client_);
    root->SetBounds(gfx::Size(10, 10));

    // Most of the layer isn't visible.
    content_ = FakeContentLayer::Create(&client_);
    content_->SetBounds(gfx::Size(100, 100));
    root->AddChild(content_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostDamageTest::SetupTree();
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    EXPECT_TRUE(result);

    int source_frame = host_impl->active_tree()->source_frame_number();
    switch (source_frame) {
      case 0:
        // The first frame has damage, so we should draw and swap.
        ++expect_swap_and_succeed_;
        break;
      case 1:
        // The second frame has no damage, so we should not draw and swap.
        break;
      case 2:
        // The third frame has damage again, so we should draw and swap.
        ++expect_swap_and_succeed_;
        break;
      case 3:
        // The fourth frame has no visible damage, so we should not draw and
        // swap.
        EndTest();
        break;
    }
    return result;
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ++did_swaps_;
    if (result)
      ++did_swap_and_succeed_;
    EXPECT_EQ(expect_swap_and_succeed_, did_swap_and_succeed_);
  }

  virtual void DidCommit() OVERRIDE {
    int next_frame = layer_tree_host()->commit_number();
    switch (next_frame) {
      case 1:
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2:
        // Cause visible damage.
        content_->SetNeedsDisplayRect(
            gfx::Rect(layer_tree_host()->device_viewport_size()));
        break;
      case 3:
        // Cause non-visible damage.
        content_->SetNeedsDisplayRect(gfx::Rect(90, 90, 10, 10));
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(4, did_swaps_);
    EXPECT_EQ(2, expect_swap_and_succeed_);
    EXPECT_EQ(expect_swap_and_succeed_, did_swap_and_succeed_);
  }

  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> content_;
  int expect_swap_and_succeed_;
  int did_swaps_;
  int did_swap_and_succeed_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDamageTestNoDamageDoesNotSwap);

class LayerTreeHostDamageTestNoDamageReadbackDoesDraw
    : public LayerTreeHostDamageTest {
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<FakeContentLayer> root = FakeContentLayer::Create(&client_);
    root->SetBounds(gfx::Size(10, 10));

    // Most of the layer isn't visible.
    content_ = FakeContentLayer::Create(&client_);
    content_->SetBounds(gfx::Size(100, 100));
    root->AddChild(content_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostDamageTest::SetupTree();
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    EXPECT_TRUE(result);

    int source_frame = host_impl->active_tree()->source_frame_number();
    switch (source_frame) {
      case 0:
        // The first frame draws and clears any damage.
        break;
      case 1: {
        // The second frame is a readback, we should have damage in the readback
        // rect, but not swap.
        RenderSurfaceImpl* root_surface =
            host_impl->active_tree()->root_layer()->render_surface();
        gfx::RectF root_damage =
            root_surface->damage_tracker()->current_damage_rect();
        root_damage.Intersect(root_surface->content_rect());
        EXPECT_TRUE(root_damage.Contains(gfx::Rect(3, 3, 1, 1)));
        break;
      }
      case 2:
        NOTREACHED();
        break;
    }
    return result;
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    int next_frame = layer_tree_host()->commit_number();
    switch (next_frame) {
      case 1: {
        char pixels[4];
        layer_tree_host()->CompositeAndReadback(static_cast<void*>(&pixels),
                                                gfx::Rect(3, 3, 1, 1));
        EndTest();
        break;
      }
    }
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> content_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDamageTestNoDamageReadbackDoesDraw);

class LayerTreeHostDamageTestForcedFullDamage : public LayerTreeHostDamageTest {
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    child_ = FakeContentLayer::Create(&client_);

    root_->SetBounds(gfx::Size(500, 500));
    child_->SetPosition(gfx::Point(100, 100));
    child_->SetBounds(gfx::Size(30, 30));

    root_->AddChild(child_);
    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostDamageTest::SetupTree();
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    EXPECT_TRUE(result);

    RenderSurfaceImpl* root_surface =
        host_impl->active_tree()->root_layer()->render_surface();
    gfx::RectF root_damage =
        root_surface->damage_tracker()->current_damage_rect();
    root_damage.Intersect(root_surface->content_rect());

    int source_frame = host_impl->active_tree()->source_frame_number();
    switch (source_frame) {
      case 0:
        // The first frame draws and clears any damage.
        EXPECT_EQ(gfx::RectF(root_surface->content_rect()).ToString(),
                  root_damage.ToString());
        EXPECT_FALSE(frame_data->has_no_damage);
        break;
      case 1:
        // If we get a frame without damage then we don't draw.
        EXPECT_EQ(gfx::RectF().ToString(), root_damage.ToString());
        EXPECT_TRUE(frame_data->has_no_damage);

        // Then we set full damage for the next frame.
        host_impl->SetFullRootLayerDamage();
        break;
      case 2:
        // The whole frame should be damaged as requested.
        EXPECT_EQ(gfx::RectF(root_surface->content_rect()).ToString(),
                  root_damage.ToString());
        EXPECT_FALSE(frame_data->has_no_damage);

        // Just a part of the next frame should be damaged.
        child_damage_rect_ = gfx::RectF(10, 11, 12, 13);
        break;
      case 3:
        // The update rect in the child should be damaged.
        EXPECT_EQ(gfx::RectF(100+10, 100+11, 12, 13).ToString(),
                  root_damage.ToString());
        EXPECT_FALSE(frame_data->has_no_damage);

        // If we damage part of the frame, but also damage the full
        // frame, then the whole frame should be damaged.
        child_damage_rect_ = gfx::RectF(10, 11, 12, 13);
        host_impl->SetFullRootLayerDamage();
        break;
      case 4:
        // The whole frame is damaged.
        EXPECT_EQ(gfx::RectF(root_surface->content_rect()).ToString(),
                  root_damage.ToString());
        EXPECT_FALSE(frame_data->has_no_damage);

        EndTest();
        break;
    }
    return result;
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    if (!TestEnded())
      layer_tree_host()->SetNeedsCommit();

    if (!child_damage_rect_.IsEmpty()) {
      child_->SetNeedsDisplayRect(child_damage_rect_);
      child_damage_rect_ = gfx::RectF();
    }
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> child_;
  gfx::RectF child_damage_rect_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDamageTestForcedFullDamage);

}  // namespace
}  // namespace cc
