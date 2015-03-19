// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

// These tests deal with picture layers.
class LayerTreeHostPictureTest : public LayerTreeTest {
 protected:
  void SetupTreeWithSinglePictureLayer(const gfx::Size& size) {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(size);

    root_picture_layer_ = FakePictureLayer::Create(&client_);
    root_picture_layer_->SetBounds(size);
    root->AddChild(root_picture_layer_);

    layer_tree_host()->SetRootLayer(root);
  }

  scoped_refptr<FakePictureLayer> root_picture_layer_;
  FakeContentLayerClient client_;
};

class LayerTreeHostPictureTestTwinLayer
    : public LayerTreeHostPictureTest {
  void SetupTree() override {
    SetupTreeWithSinglePictureLayer(gfx::Size(1, 1));
  }

  void BeginTest() override {
    activates_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Activate while there are pending and active twins in place.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2:
        // Drop the picture layer from the tree so the activate will have an
        // active layer without a pending twin.
        layer_tree_host()->root_layer()->children()[0]->RemoveFromParent();
        break;
      case 3: {
        // Add a new picture layer so the activate will have a pending layer
        // without an active twin.
        scoped_refptr<FakePictureLayer> picture =
            FakePictureLayer::Create(&client_);
        layer_tree_host()->root_layer()->AddChild(picture);
        break;
      }
      case 4:
        // Active while there are pending and active twins again.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 5:
        EndTest();
        break;
    }
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* pending_root_impl = impl->pending_tree()->root_layer();
    LayerImpl* active_root_impl = impl->active_tree()->root_layer();

    if (pending_root_impl->children().empty()) {
      EXPECT_EQ(2, activates_);
      return;
    }

    FakePictureLayerImpl* pending_picture_impl =
        static_cast<FakePictureLayerImpl*>(pending_root_impl->children()[0]);

    if (!active_root_impl) {
      EXPECT_EQ(0, activates_);
      EXPECT_EQ(nullptr, pending_picture_impl->GetPendingOrActiveTwinLayer());
      return;
    }

    if (active_root_impl->children().empty()) {
      EXPECT_EQ(3, activates_);
      EXPECT_EQ(nullptr, pending_picture_impl->GetPendingOrActiveTwinLayer());
      return;
    }

    FakePictureLayerImpl* active_picture_impl =
        static_cast<FakePictureLayerImpl*>(active_root_impl->children()[0]);

    // After the first activation, when we commit again, we'll have a pending
    // and active layer. Then we recreate a picture layer in the 4th activate
    // and the next commit will have a pending and active twin again.
    EXPECT_TRUE(activates_ == 1 || activates_ == 4) << activates_;

    EXPECT_EQ(pending_picture_impl,
              active_picture_impl->GetPendingOrActiveTwinLayer());
    EXPECT_EQ(active_picture_impl,
              pending_picture_impl->GetPendingOrActiveTwinLayer());
    EXPECT_EQ(nullptr, active_picture_impl->GetRecycledTwinLayer());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* active_root_impl = impl->active_tree()->root_layer();
    LayerImpl* recycle_root_impl = impl->recycle_tree()->root_layer();

    if (active_root_impl->children().empty()) {
      EXPECT_EQ(2, activates_);
    } else {
      FakePictureLayerImpl* active_picture_impl =
          static_cast<FakePictureLayerImpl*>(active_root_impl->children()[0]);
      FakePictureLayerImpl* recycle_picture_impl =
          static_cast<FakePictureLayerImpl*>(recycle_root_impl->children()[0]);

      EXPECT_EQ(nullptr, active_picture_impl->GetPendingOrActiveTwinLayer());
      EXPECT_EQ(recycle_picture_impl,
                active_picture_impl->GetRecycledTwinLayer());
    }

    ++activates_;
  }

  void AfterTest() override { EXPECT_EQ(5, activates_); }

  int activates_;
};

// There is no pending layers in single thread mode.
MULTI_THREAD_IMPL_TEST_F(LayerTreeHostPictureTestTwinLayer);

class LayerTreeHostPictureTestResizeViewportWithGpuRaster
    : public LayerTreeHostPictureTest {
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->gpu_rasterization_forced = true;
  }

  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(768, 960));

    client_.set_fill_with_nonsolid_color(true);
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(768, 960));
    root->AddChild(picture_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostPictureTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* child = impl->sync_tree()->root_layer()->children()[0];
    FakePictureLayerImpl* picture_impl =
        static_cast<FakePictureLayerImpl*>(child);
    gfx::Size tile_size =
        picture_impl->HighResTiling()->TileAt(0, 0)->content_rect().size();

    switch (impl->sync_tree()->source_frame_number()) {
      case 0:
        tile_size_ = tile_size;
        // GPU Raster picks a tile size based on the viewport size.
        EXPECT_EQ(gfx::Size(768, 256), tile_size);
        break;
      case 1:
        // When the viewport changed size, the new frame's tiles should change
        // along with it.
        EXPECT_NE(gfx::Size(768, 256), tile_size);
    }
  }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Change the picture layer's size along with the viewport, so it will
        // consider picking a new tile size.
        picture_->SetBounds(gfx::Size(768, 1056));
        layer_tree_host()->SetViewportSize(gfx::Size(768, 1056));
        break;
      case 2:
        EndTest();
    }
  }

  void AfterTest() override {}

  gfx::Size tile_size_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_IMPL_TEST_F(
    LayerTreeHostPictureTestResizeViewportWithGpuRaster);

class LayerTreeHostPictureTestChangeLiveTilesRectWithRecycleTree
    : public LayerTreeHostPictureTest {
  void SetupTree() override {
    frame_ = 0;
    did_post_commit_ = false;

    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(100, 100));

    // The layer is big enough that the live tiles rect won't cover the full
    // layer.
    client_.set_fill_with_nonsolid_color(true);
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(100, 100000));
    root->AddChild(picture_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostPictureTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* child = impl->active_tree()->root_layer()->children()[0];
    FakePictureLayerImpl* picture_impl =
        static_cast<FakePictureLayerImpl*>(child);
    FakePictureLayerImpl* recycled_impl = static_cast<FakePictureLayerImpl*>(
        picture_impl->GetRecycledTwinLayer());

    switch (++frame_) {
      case 1: {
        PictureLayerTiling* tiling = picture_impl->HighResTiling();
        PictureLayerTiling* recycled_tiling = recycled_impl->HighResTiling();
        int num_tiles_y = tiling->TilingDataForTesting().num_tiles_y();

        // There should be tiles at the top of the picture layer but not at the
        // bottom.
        EXPECT_TRUE(tiling->TileAt(0, 0));
        EXPECT_FALSE(tiling->TileAt(0, num_tiles_y));

        // The recycled tiling matches it.
        EXPECT_TRUE(recycled_tiling->TileAt(0, 0));
        EXPECT_FALSE(recycled_tiling->TileAt(0, num_tiles_y));

        // The live tiles rect matches on the recycled tree.
        EXPECT_EQ(tiling->live_tiles_rect(),
                  recycled_tiling->live_tiles_rect());

        // Make the bottom of the layer visible.
        picture_impl->SetPosition(gfx::PointF(0.f, -100000.f + 100.f));
        impl->SetNeedsRedraw();
        break;
      }
      case 2: {
        PictureLayerTiling* tiling = picture_impl->HighResTiling();
        PictureLayerTiling* recycled_tiling = recycled_impl->HighResTiling();

        // There not be tiles at the top of the layer now.
        EXPECT_FALSE(tiling->TileAt(0, 0));

        // The recycled twin tiling should not have unshared tiles at the top
        // either.
        EXPECT_FALSE(recycled_tiling->TileAt(0, 0));

        // The live tiles rect matches on the recycled tree.
        EXPECT_EQ(tiling->live_tiles_rect(),
                  recycled_tiling->live_tiles_rect());

        // Make the top of the layer visible again.
        picture_impl->SetPosition(gfx::PointF());
        impl->SetNeedsRedraw();
        break;
      }
      case 3: {
        PictureLayerTiling* tiling = picture_impl->HighResTiling();
        PictureLayerTiling* recycled_tiling = recycled_impl->HighResTiling();
        int num_tiles_y = tiling->TilingDataForTesting().num_tiles_y();

        // There should be tiles at the top of the picture layer again.
        EXPECT_TRUE(tiling->TileAt(0, 0));
        EXPECT_FALSE(tiling->TileAt(0, num_tiles_y));

        // The recycled tiling should also have tiles at the top.
        EXPECT_TRUE(recycled_tiling->TileAt(0, 0));
        EXPECT_FALSE(recycled_tiling->TileAt(0, num_tiles_y));

        // The live tiles rect matches on the recycled tree.
        EXPECT_EQ(tiling->live_tiles_rect(),
                  recycled_tiling->live_tiles_rect());

        // Make a new main frame without changing the picture layer at all, so
        // it won't need to update or push properties.
        did_post_commit_ = true;
        PostSetNeedsCommitToMainThread();
        break;
      }
    }
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* child = impl->sync_tree()->root_layer()->children()[0];
    FakePictureLayerImpl* picture_impl =
        static_cast<FakePictureLayerImpl*>(child);
    PictureLayerTiling* tiling = picture_impl->HighResTiling();
    int num_tiles_y = tiling->TilingDataForTesting().num_tiles_y();

    // The pending layer should always have tiles at the top of it each commit.
    // The tile is part of the required for activation set so it should exist.
    EXPECT_TRUE(tiling->TileAt(0, 0));
    EXPECT_FALSE(tiling->TileAt(0, num_tiles_y));

    if (did_post_commit_)
      EndTest();
  }

  void AfterTest() override {}

  int frame_;
  bool did_post_commit_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

// Multi-thread only since there is no recycle tree in single thread.
MULTI_THREAD_IMPL_TEST_F(
    LayerTreeHostPictureTestChangeLiveTilesRectWithRecycleTree);

class LayerTreeHostPictureTestRSLLMembership : public LayerTreeHostPictureTest {
  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(100, 100));

    child_ = Layer::Create();
    root->AddChild(child_);

    // Don't be solid color so the layer has tilings/tiles.
    client_.set_fill_with_nonsolid_color(true);
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(100, 100));
    child_->AddChild(picture_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostPictureTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root = impl->sync_tree()->root_layer();
    LayerImpl* child = root->children()[0];
    LayerImpl* gchild = child->children()[0];
    FakePictureLayerImpl* picture = static_cast<FakePictureLayerImpl*>(gchild);

    switch (impl->sync_tree()->source_frame_number()) {
      case 0:
        // On 1st commit the layer has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 1:
        // On 2nd commit, the layer is transparent, but its tilings are left
        // there.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 2:
        // On 3rd commit, the layer is visible again, so has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* child = root->children()[0];
    LayerImpl* gchild = child->children()[0];
    FakePictureLayerImpl* picture = static_cast<FakePictureLayerImpl*>(gchild);

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // On 1st commit the layer has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 1:
        // On 2nd commit, the layer is transparent, but its tilings are left
        // there.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 2:
        // On 3rd commit, the layer is visible again, so has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        EndTest();
    }
  }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // For the 2nd commit, change opacity to 0 so that the layer will not be
        // part of the visible frame.
        child_->SetOpacity(0.f);
        break;
      case 2:
        // For the 3rd commit, change opacity to 1 so that the layer will again
        // be part of the visible frame.
        child_->SetOpacity(1.f);
    }
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
  scoped_refptr<Layer> child_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_IMPL_TEST_F(LayerTreeHostPictureTestRSLLMembership);

class LayerTreeHostPictureTestRSLLMembershipWithScale
    : public LayerTreeHostPictureTest {
  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(100, 100));

    pinch_ = Layer::Create();
    pinch_->SetBounds(gfx::Size(500, 500));
    pinch_->SetScrollClipLayerId(root->id());
    pinch_->SetIsContainerForFixedPositionLayers(true);
    root->AddChild(pinch_);

    // Don't be solid color so the layer has tilings/tiles.
    client_.set_fill_with_nonsolid_color(true);
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(100, 100));
    pinch_->AddChild(picture_);

    layer_tree_host()->RegisterViewportLayers(NULL, root, pinch_, pinch_);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 1.f, 4.f);
    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostPictureTest::SetupTree();
  }

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->layer_transforms_should_scale_layer_contents = true;
  }

  void BeginTest() override {
    frame_ = 0;
    draws_in_frame_ = 0;
    last_frame_drawn_ = -1;
    PostSetNeedsCommitToMainThread();
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root = impl->sync_tree()->root_layer();
    LayerImpl* pinch = root->children()[0];
    LayerImpl* gchild = pinch->children()[0];
    FakePictureLayerImpl* picture = static_cast<FakePictureLayerImpl*>(gchild);

    switch (frame_) {
      case 0:
        // On 1st commit the layer has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 1:
        // On 2nd commit, the layer is transparent, so does not have tilings.
        EXPECT_EQ(0u, picture->tilings()->num_tilings());
        break;
      case 2:
        // On 3rd commit, the layer is visible again, so has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* pinch = root->children()[0];
    LayerImpl* gchild = pinch->children()[0];
    FakePictureLayerImpl* picture = static_cast<FakePictureLayerImpl*>(gchild);

    if (frame_ != last_frame_drawn_)
      draws_in_frame_ = 0;
    ++draws_in_frame_;
    last_frame_drawn_ = frame_;

    switch (frame_) {
      case 0:
        if (draws_in_frame_ == 1) {
          // On 1st commit the layer has tilings.
          EXPECT_GT(picture->tilings()->num_tilings(), 0u);
          EXPECT_EQ(1.f, picture->HighResTiling()->contents_scale());

          // Pinch zoom in to change the scale on the active tree.
          impl->PinchGestureBegin();
          impl->PinchGestureUpdate(2.f, gfx::Point(1, 1));
          impl->PinchGestureEnd();
        } else if (picture->tilings()->num_tilings() == 1) {
          // If the pinch gesture caused a commit we could get here with a
          // pending tree.
          EXPECT_FALSE(impl->pending_tree());
          // The active layer now has only a 2.f scale tiling, which means the
          // recycled layer's tiling is destroyed.
          EXPECT_EQ(2.f, picture->HighResTiling()->contents_scale());
          EXPECT_EQ(0u, picture->GetRecycledTwinLayer()
                            ->picture_layer_tiling_set()
                            ->num_tilings());

          ++frame_;
          MainThreadTaskRunner()->PostTask(
              FROM_HERE,
              base::Bind(
                  &LayerTreeHostPictureTestRSLLMembershipWithScale::NextStep,
                  base::Unretained(this)));
        }
        break;
      case 1:
        EXPECT_EQ(1, draws_in_frame_);
        // On 2nd commit, the layer is transparent, so does not create
        // tilings. Since the 1.f tiling was destroyed in the recycle tree, it
        // has no tilings left. This is propogated to the active tree.
        EXPECT_EQ(0u, picture->picture_layer_tiling_set()->num_tilings());
        EXPECT_EQ(0u, picture->GetRecycledTwinLayer()
                          ->picture_layer_tiling_set()
                          ->num_tilings());
        ++frame_;
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(
                &LayerTreeHostPictureTestRSLLMembershipWithScale::NextStep,
                base::Unretained(this)));
        break;
      case 2:
        EXPECT_EQ(1, draws_in_frame_);
        // On 3rd commit, the layer is visible again, so has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        EndTest();
    }
  }

  void NextStep() {
    switch (frame_) {
      case 1:
        // For the 2nd commit, change opacity to 0 so that the layer will not be
        // part of the visible frame.
        pinch_->SetOpacity(0.f);
        break;
      case 2:
        // For the 3rd commit, change opacity to 1 so that the layer will again
        // be part of the visible frame.
        pinch_->SetOpacity(1.f);
        break;
    }
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
  scoped_refptr<Layer> pinch_;
  scoped_refptr<FakePictureLayer> picture_;
  int frame_;
  int draws_in_frame_;
  int last_frame_drawn_;
};

// Multi-thread only because in single thread you can't pinch zoom on the
// compositor thread.
// Disabled due to flakiness. See http://crbug.com/460581
// MULTI_THREAD_IMPL_TEST_F(LayerTreeHostPictureTestRSLLMembershipWithScale);

}  // namespace
}  // namespace cc
