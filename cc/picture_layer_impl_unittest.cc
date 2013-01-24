// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_impl.h"

#include "cc/layer_tree_impl.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {
namespace {

class FakePicture : public Picture {
 public:
  static scoped_refptr<Picture> Create(gfx::Rect layer_rect) {
    scoped_refptr<FakePicture> picture(new FakePicture(layer_rect));

    FakeContentLayerClient client;
    RenderingStats stats;
    picture->Record(&client, stats);

    return picture;
  }

 protected:
  FakePicture(gfx::Rect layer_rect)
      : Picture(layer_rect) {
  }

  ~FakePicture() {
  }
};

class TestablePictureLayerImpl : public PictureLayerImpl {
 public:
  static scoped_ptr<TestablePictureLayerImpl> create(
      LayerTreeImpl* treeImpl,
      int id,
      scoped_refptr<PicturePileImpl> pile)
  {
    return make_scoped_ptr(new TestablePictureLayerImpl(treeImpl, id, pile));
  }

  PictureLayerTilingSet& tilings() { return *tilings_; }
  Region& invalidation() { return invalidation_; }

  using PictureLayerImpl::AddTiling;

 private:
  TestablePictureLayerImpl(
      LayerTreeImpl* treeImpl,
      int id,
      scoped_refptr<PicturePileImpl> pile)
      : PictureLayerImpl(treeImpl, id) {
    pile_ = pile;
    setBounds(pile_->size());
    CreateTilingSet();
  }
};

class TestablePicturePileImpl : public PicturePileImpl {
 public:
  static scoped_refptr<TestablePicturePileImpl> CreateFilledPile(
      gfx::Size tile_size,
      gfx::Size layer_bounds) {
    scoped_refptr<TestablePicturePileImpl> pile(new TestablePicturePileImpl());
    pile->tiling().SetTotalSize(layer_bounds);
    pile->tiling().SetMaxTextureSize(tile_size);
    for (int x = 0; x < pile->tiling().num_tiles_x(); ++x) {
      for (int y = 0; y < pile->tiling().num_tiles_y(); ++y)
        pile->AddRecordingAt(x, y);
    }
    pile->UpdateRecordedRegion();
    return pile;
  }

  static scoped_refptr<TestablePicturePileImpl> CreateEmptyPile(
      gfx::Size tile_size,
      gfx::Size layer_bounds) {
    scoped_refptr<TestablePicturePileImpl> pile(new TestablePicturePileImpl());
    pile->tiling().SetTotalSize(layer_bounds);
    pile->tiling().SetMaxTextureSize(tile_size);
    pile->UpdateRecordedRegion();
    return pile;
  }

  TilingData& tiling() { return tiling_; }

  void AddRecordingAt(int x, int y) {
    EXPECT_GE(x, 0);
    EXPECT_GE(y, 0);
    EXPECT_LT(x, tiling_.num_tiles_x());
    EXPECT_LT(y, tiling_.num_tiles_y());

    if (HasRecordingAt(x, y))
      return;
    gfx::Rect bounds(tiling().TileBounds(x, y));
    scoped_refptr<Picture> picture(FakePicture::Create(bounds));
    picture_list_map_[std::pair<int, int>(x, y)].push_back(picture);
    EXPECT_TRUE(HasRecordingAt(x, y));
  }

  void RemoveRecordingAt(int x, int y) {
    EXPECT_GE(x, 0);
    EXPECT_GE(y, 0);
    EXPECT_LT(x, tiling_.num_tiles_x());
    EXPECT_LT(y, tiling_.num_tiles_y());

    if (!HasRecordingAt(x, y))
      return;
    picture_list_map_.erase(std::pair<int, int>(x, y));
    EXPECT_FALSE(HasRecordingAt(x, y));
  }

 protected:
  ~TestablePicturePileImpl() {
  }
};

class ImplSidePaintingSettings : public LayerTreeSettings {
 public:
  ImplSidePaintingSettings() {
    implSidePainting = true;
  }
};

class PictureLayerImplTest : public testing::Test {
 public:
  PictureLayerImplTest()
      : host_impl_(ImplSidePaintingSettings(), &proxy_),
        id_(7) {
    host_impl_.initializeRenderer(createFakeOutputSurface());
  }

  ~PictureLayerImplTest() {
  }

  void SetupTrees(
      scoped_refptr<PicturePileImpl> pending_pile,
      scoped_refptr<PicturePileImpl> active_pile,
      const Region& invalidation) {
    SetupPendingTree(active_pile);
    host_impl_.activatePendingTree();

    active_layer_ = static_cast<TestablePictureLayerImpl*>(
        host_impl_.activeTree()->LayerById(id_));
    active_layer_->AddTiling(2.3f);
    active_layer_->AddTiling(1.0f);
    active_layer_->AddTiling(0.5f);

    SetupPendingTree(pending_pile);
    pending_layer_ = static_cast<TestablePictureLayerImpl*>(
        host_impl_.pendingTree()->LayerById(id_));
    pending_layer_->invalidation() = invalidation;
    pending_layer_->SyncFromActiveLayer();
  }

  void SetupPendingTree(
      scoped_refptr<PicturePileImpl> pile) {
    host_impl_.createPendingTree();
    LayerTreeImpl* pending_tree = host_impl_.pendingTree();
    // Clear recycled tree.
    pending_tree->DetachLayerTree();

    scoped_ptr<TestablePictureLayerImpl> pending_layer =
        TestablePictureLayerImpl::create(pending_tree, id_, pile);
    pending_layer->setDrawsContent(true);
    pending_tree->SetRootLayer(pending_layer.PassAs<LayerImpl>());
  }

  static void VerifyAllTilesExistAndHavePile(
      const PictureLayerTiling* tiling,
      PicturePileImpl* pile) {
    for (PictureLayerTiling::Iterator iter(tiling,
                                           tiling->contents_scale(),
                                           tiling->ContentRect());
         iter; ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_EQ(pile, iter->picture_pile());
    }
  }

 protected:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  int id_;
  TestablePictureLayerImpl* pending_layer_;
  TestablePictureLayerImpl* active_layer_;

  DISALLOW_COPY_AND_ASSIGN(PictureLayerImplTest);
};

TEST_F(PictureLayerImplTest, cloneNoInvalidation) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  Region invalidation;
  SetupTrees(pending_pile, active_pile, invalidation);

  EXPECT_EQ(pending_layer_->tilings().num_tilings(),
            active_layer_->tilings().num_tilings());

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  EXPECT_GT(tilings.num_tilings(), 0u);
  for (size_t i = 0; i < tilings.num_tilings(); ++i)
    VerifyAllTilesExistAndHavePile(tilings.tiling_at(i), active_pile.get());
}

TEST_F(PictureLayerImplTest, clonePartialInvalidation) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);
  gfx::Rect layer_invalidation(150, 200, 30, 180);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  Region invalidation(layer_invalidation);
  SetupTrees(pending_pile, active_pile, layer_invalidation);

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  for (size_t i = 0; i < tilings.num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings.tiling_at(i);
    gfx::Rect content_invalidation = gfx::ToEnclosingRect(gfx::ScaleRect(
        layer_invalidation,
        tiling->contents_scale()));
    for (PictureLayerTiling::Iterator iter(tiling,
                                           tiling->contents_scale(),
                                           tiling->ContentRect());
         iter; ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_FALSE(iter.geometry_rect().IsEmpty());
      if (iter.geometry_rect().Intersects(content_invalidation))
        EXPECT_EQ(pending_pile, iter->picture_pile());
      else
        EXPECT_EQ(active_pile, iter->picture_pile());
    }
  }
}

TEST_F(PictureLayerImplTest, cloneFullInvalidation) {
  gfx::Size tile_size(90, 80);
  gfx::Size layer_bounds(300, 500);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  Region invalidation((gfx::Rect(layer_bounds)));
  SetupTrees(pending_pile, active_pile, invalidation);

  EXPECT_EQ(pending_layer_->tilings().num_tilings(),
            active_layer_->tilings().num_tilings());

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  EXPECT_GT(tilings.num_tilings(), 0u);
  for (size_t i = 0; i < tilings.num_tilings(); ++i)
    VerifyAllTilesExistAndHavePile(tilings.tiling_at(i), pending_pile.get());
}

TEST_F(PictureLayerImplTest, noInvalidationBoundsChange) {
  gfx::Size tile_size(90, 80);
  gfx::Size active_layer_bounds(300, 500);
  gfx::Size pending_layer_bounds(400, 800);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size,
                                                pending_layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, active_layer_bounds);

  Region invalidation;
  SetupTrees(pending_pile, active_pile, invalidation);

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  for (size_t i = 0; i < tilings.num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings.tiling_at(i);
    gfx::Rect active_content_bounds = gfx::ToEnclosingRect(gfx::ScaleRect(
        gfx::Rect(active_layer_bounds),
        tiling->contents_scale()));
    for (PictureLayerTiling::Iterator iter(tiling,
                                           tiling->contents_scale(),
                                           tiling->ContentRect());
         iter; ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_FALSE(iter.geometry_rect().IsEmpty());
      if (iter.geometry_rect().right() >= active_content_bounds.width() ||
          iter.geometry_rect().bottom() >= active_content_bounds.height()) {
        EXPECT_EQ(pending_pile, iter->picture_pile());
      } else {
        EXPECT_EQ(active_pile, iter->picture_pile());
      }
    }
  }
}

TEST_F(PictureLayerImplTest, addTilesFromNewRecording) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);

  // Fill in some of active pile, but more of pending pile.
  int hole_count = 0;
  for (int x = 0; x < active_pile->tiling().num_tiles_x(); ++x) {
    for (int y = 0; y < active_pile->tiling().num_tiles_y(); ++y) {
      if ((x + y) % 2) {
        pending_pile->AddRecordingAt(x, y);
        active_pile->AddRecordingAt(x, y);
      } else {
        hole_count++;
        if (hole_count % 2)
          pending_pile->AddRecordingAt(x, y);
      }
    }
  }

  Region invalidation;
  SetupTrees(pending_pile, active_pile, invalidation);

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  for (size_t i = 0; i < tilings.num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings.tiling_at(i);

    for (PictureLayerTiling::Iterator iter(tiling,
                                           tiling->contents_scale(),
                                           tiling->ContentRect());
         iter; ++iter) {
      EXPECT_FALSE(iter.geometry_rect().IsEmpty());
      // Ensure there is a recording for this tile.
      gfx::Rect layer_rect = gfx::ToEnclosingRect(
          gfx::ScaleRect(iter.geometry_rect(), 1.f / tiling->contents_scale()));

      bool in_pending = pending_pile->recorded_region().Contains(layer_rect);
      bool in_active = active_pile->recorded_region().Contains(layer_rect);

      if (in_pending && !in_active)
        EXPECT_EQ(pending_pile, iter->picture_pile());
      else if (in_active)
        EXPECT_EQ(active_pile, iter->picture_pile());
      else
        EXPECT_FALSE(*iter);
    }
  }
}

}  // namespace
}  // namespace cc
