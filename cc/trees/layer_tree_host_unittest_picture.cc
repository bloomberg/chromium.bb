// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/layers/solid_color_layer.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_content_layer_impl.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

// These tests deal with picture layers.
class LayerTreeHostPictureTest : public LayerTreeTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // PictureLayer can only be used with impl side painting enabled.
    settings->impl_side_painting = true;
  }
};

class LayerTreeHostPictureTestTwinLayer
    : public LayerTreeHostPictureTest {
  virtual void SetupTree() OVERRIDE {
    LayerTreeHostPictureTest::SetupTree();

    scoped_refptr<FakePictureLayer> picture =
        FakePictureLayer::Create(&client_);
    layer_tree_host()->root_layer()->AddChild(picture);
  }

  virtual void BeginTest() OVERRIDE {
    activates_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 2:
        // Drop the picture layer from the tree.
        layer_tree_host()->root_layer()->children()[0]->RemoveFromParent();
        break;
      case 3:
        // Add a new picture layer.
        scoped_refptr<FakePictureLayer> picture =
            FakePictureLayer::Create(&client_);
        layer_tree_host()->root_layer()->AddChild(picture);
        break;
    }
  }

  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
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
      EXPECT_EQ(NULL, pending_picture_impl->twin_layer());
      return;
    }

    if (active_root_impl->children().empty()) {
      EXPECT_EQ(3, activates_);
      EXPECT_EQ(NULL, pending_picture_impl->twin_layer());
      return;
    }

    FakePictureLayerImpl* active_picture_impl =
        static_cast<FakePictureLayerImpl*>(active_root_impl->children()[0]);

    // After the first activation, when we commit again, we'll have a pending
    // and active layer. Then we recreate a picture layer in the 4th activate
    // and the next commit will have a pending and active twin again.
    EXPECT_TRUE(activates_ == 1 || activates_ == 4);

    EXPECT_EQ(pending_picture_impl, active_picture_impl->twin_layer());
    EXPECT_EQ(active_picture_impl, pending_picture_impl->twin_layer());
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* active_root_impl = impl->active_tree()->root_layer();

    if (active_root_impl->children().empty()) {
      EXPECT_EQ(2, activates_);
    } else {
      FakePictureLayerImpl* active_picture_impl =
          static_cast<FakePictureLayerImpl*>(active_root_impl->children()[0]);

      EXPECT_EQ(NULL, active_picture_impl->twin_layer());
    }

    ++activates_;
    if (activates_ <= 4)
      PostSetNeedsCommitToMainThread();
    else
      EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  int activates_;
};

MULTI_THREAD_TEST_F(LayerTreeHostPictureTestTwinLayer);

class LayerTreeHostPictureNoUpdateTileProprityForZeroOpacityLayer
    : public LayerTreeHostPictureTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    parent_layer_ = FakeContentLayer::Create(&parent_client_);
    parent_layer_->SetBounds(gfx::Size(40, 40));
    parent_layer_->SetOpacity(0.f);
    parent_layer_->SetIsDrawable(true);

    scoped_refptr<Layer> root = SolidColorLayer::Create();
    root->SetBounds(gfx::Size(60, 60));
    root->SetOpacity(1.f);
    root->SetIsDrawable(true);
    root->AddChild(parent_layer_);
    layer_tree_host()->SetRootLayer(root);

    wheel_handler_ = FakeContentLayer::Create(NULL);
    wheel_handler_->SetBounds(gfx::Size(10, 10));
    wheel_handler_->SetHaveWheelEventHandlers(true);
    wheel_handler_->SetIsDrawable(true);
    parent_layer_->AddChild(wheel_handler_);

    picture_ = FakePictureLayer::Create(&picture_client_);
    picture_->SetBounds(gfx::Size(30, 30));
    picture_->SetPosition(gfx::Point(10, 0));
    picture_->SetIsDrawable(true);
    parent_layer_->AddChild(picture_);

    LayerTreeHostPictureTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostPictureTest::DrawLayersOnThread(impl);
    FakeContentLayerImpl* parent =
          static_cast<FakeContentLayerImpl*>(impl->RootLayer()->children()[0]);
    EXPECT_TRUE(parent->DrawsContent());
    EXPECT_FALSE(parent->opacity());
    EXPECT_EQ(0u, parent->append_quads_count());
    ASSERT_EQ(2u, parent->children().size());

    FakeContentLayerImpl* child =
        static_cast<FakeContentLayerImpl*>(parent->children()[0]);
    EXPECT_TRUE(child->opacity());
    EXPECT_EQ(0u, child->append_quads_count());

    FakePictureLayerImpl* picimpl =
        static_cast<FakePictureLayerImpl*>(parent->children()[1]);
    EXPECT_TRUE(picimpl->opacity());
    EXPECT_EQ(0u, picimpl->append_quads_count());
    EXPECT_EQ(0u, picimpl->update_tile_priorities_count());

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient parent_client_;
  FakeContentLayerClient picture_client_;
  scoped_refptr<FakeContentLayer> parent_layer_;
  scoped_refptr<FakeContentLayer> wheel_handler_;
  scoped_refptr<FakePictureLayer> picture_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostPictureNoUpdateTileProprityForZeroOpacityLayer);

}  // namespace
}  // namespace cc
