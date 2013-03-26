// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include "cc/layers/picture_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {
namespace {

class TestablePictureLayerImpl : public PictureLayerImpl {
 public:
  static scoped_ptr<TestablePictureLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<PicturePileImpl> pile)
  {
    return make_scoped_ptr(new TestablePictureLayerImpl(tree_impl, id, pile));
  }

  PictureLayerTilingSet& tilings() { return *tilings_; }
  Region& invalidation() { return invalidation_; }

  virtual gfx::Size CalculateTileSize(
      gfx::Size current_tile_size,
      gfx::Size content_bounds) OVERRIDE {
    if (current_tile_size.IsEmpty())
      return gfx::Size(100, 100);
    return current_tile_size;
  }

  using PictureLayerImpl::AddTiling;
  using PictureLayerImpl::CleanUpTilingsOnActiveLayer;

 private:
  TestablePictureLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<PicturePileImpl> pile)
      : PictureLayerImpl(tree_impl, id) {
    pile_ = pile;
    SetBounds(pile_->size());
    CreateTilingSet();
  }
};

class ImplSidePaintingSettings : public LayerTreeSettings {
 public:
  ImplSidePaintingSettings() {
    impl_side_painting = true;
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
    pile->SetTileGridSize(ImplSidePaintingSettings().default_tile_size);
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
    pile->SetTileGridSize(ImplSidePaintingSettings().default_tile_size);
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
    scoped_refptr<Picture> picture(Picture::Create(bounds));
    picture->Record(&client_, NULL, tile_grid_info_);
    picture_list_map_[std::pair<int, int>(x, y)].push_back(picture);
    EXPECT_TRUE(HasRecordingAt(x, y));

    UpdateRecordedRegion();
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

    UpdateRecordedRegion();
  }

  void add_draw_rect(const gfx::Rect& rect) {
    client_.add_draw_rect(rect);
  }

 protected:
  TestablePicturePileImpl() : PicturePileImpl(false) {}

  virtual ~TestablePicturePileImpl() {}

  FakeContentLayerClient client_;
};

class MockCanvas : public SkCanvas {
 public:
  explicit MockCanvas(SkDevice* device) : SkCanvas(device) {}

  virtual void drawRect(const SkRect& rect, const SkPaint& paint) OVERRIDE {
    // Capture calls before SkCanvas quickReject() kicks in.
    rects_.push_back(rect);
  }

  std::vector<SkRect> rects_;
};

class PictureLayerImplTest : public testing::Test {
 public:
  PictureLayerImplTest()
      : host_impl_(ImplSidePaintingSettings(), &proxy_),
        id_(7) {
    host_impl_.InitializeRenderer(CreateFakeOutputSurface());
  }

  virtual ~PictureLayerImplTest() {
  }

  void SetupTrees(
      scoped_refptr<PicturePileImpl> pending_pile,
      scoped_refptr<PicturePileImpl> active_pile) {
    SetupPendingTree(active_pile);
    host_impl_.ActivatePendingTree();

    active_layer_ = static_cast<TestablePictureLayerImpl*>(
        host_impl_.active_tree()->LayerById(id_));

    SetupPendingTree(pending_pile);
    pending_layer_ = static_cast<TestablePictureLayerImpl*>(
        host_impl_.pending_tree()->LayerById(id_));
  }

  void AddDefaultTilingsWithInvalidation(const Region& invalidation) {
    active_layer_->AddTiling(2.3f);
    active_layer_->AddTiling(1.0f);
    active_layer_->AddTiling(0.5f);
    pending_layer_->invalidation() = invalidation;
    pending_layer_->SyncFromActiveLayer();
  }

  void SetupPendingTree(
      scoped_refptr<PicturePileImpl> pile) {
    host_impl_.CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl_.pending_tree();
    // Clear recycled tree.
    pending_tree->DetachLayerTree();

    scoped_ptr<TestablePictureLayerImpl> pending_layer =
        TestablePictureLayerImpl::Create(pending_tree, id_, pile);
    pending_layer->SetDrawsContent(true);
    pending_tree->SetRootLayer(pending_layer.PassAs<LayerImpl>());
  }

  static void VerifyAllTilesExistAndHavePile(
      const PictureLayerTiling* tiling,
      PicturePileImpl* pile) {
    for (PictureLayerTiling::Iterator
             iter(tiling, tiling->contents_scale(), tiling->ContentRect());
         iter;
         ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_EQ(pile, iter->picture_pile());
    }
  }

  void SetContentsScaleOnBothLayers(float scale, bool animating_transform) {
    float result_scale_x, result_scale_y;
    gfx::Size result_bounds;
    pending_layer_->CalculateContentsScale(
        scale, animating_transform,
        &result_scale_x, &result_scale_y, &result_bounds);
    active_layer_->CalculateContentsScale(
        scale, animating_transform,
        &result_scale_x, &result_scale_y, &result_bounds);
  }

 protected:
  void TestTileGridAlignmentCommon() {
    // Layer to span 4 raster tiles in x and in y
    ImplSidePaintingSettings settings;
    gfx::Size layer_size(
        settings.default_tile_size.width() * 7 / 2,
        settings.default_tile_size.height() * 7 / 2);

    scoped_refptr<TestablePicturePileImpl> pending_pile =
        TestablePicturePileImpl::CreateFilledPile(layer_size, layer_size);
    scoped_refptr<TestablePicturePileImpl> active_pile =
        TestablePicturePileImpl::CreateFilledPile(layer_size, layer_size);

    SetupTrees(pending_pile, active_pile);

    host_impl_.active_tree()->SetPageScaleFactorAndLimits(1.f, 1.f, 1.f);
    float result_scale_x, result_scale_y;
    gfx::Size result_bounds;
    active_layer_->CalculateContentsScale(
        1.f, false, &result_scale_x, &result_scale_y, &result_bounds);

    // Add 1x1 rects at the centers of each tile, then re-record pile contents
    std::vector<Tile*> tiles =
        active_layer_->tilings().tiling_at(0)->AllTilesForTesting();
    EXPECT_EQ(16, tiles.size());
    std::vector<SkRect> rects;
    std::vector<Tile*>::const_iterator tile_iter;
    for (tile_iter = tiles.begin(); tile_iter < tiles.end(); tile_iter++) {
      gfx::Point tile_center = (*tile_iter)->content_rect().CenterPoint();
      gfx::Rect rect(tile_center.x(), tile_center.y(), 1, 1);
      active_pile->add_draw_rect(rect);
      rects.push_back(SkRect::MakeXYWH(rect.x(), rect.y(), 1, 1));
    }
    // Force re-record with newly injected content
    active_pile->RemoveRecordingAt(0, 0);
    active_pile->AddRecordingAt(0, 0);

    SkBitmap store;
    store.setConfig(SkBitmap::kNo_Config, 1000, 1000);
    SkDevice device(store);

    std::vector<SkRect>::const_iterator rect_iter = rects.begin();
    for (tile_iter = tiles.begin(); tile_iter < tiles.end(); tile_iter++) {
      MockCanvas mock_canvas(&device);
      active_pile->Raster(&mock_canvas, (*tile_iter)->content_rect(), 1.0f);

      // This test verifies that when drawing the contents of a specific tile
      // at content scale 1.0, the playback canvas never receives content from
      // neighboring tiles which indicates that the tile grid embedded in
      // SkPicture is perfectly aligned with the compositor's tiles.
      // Note: There are two rects: the initial clear and the explicitly
      // recorded rect. We only care about the second one.
      EXPECT_EQ(2, mock_canvas.rects_.size());
      EXPECT_EQ(*rect_iter, mock_canvas.rects_[1]);
      rect_iter++;
    }
  }

  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  int id_;
  TestablePictureLayerImpl* pending_layer_;
  TestablePictureLayerImpl* active_layer_;

  DISALLOW_COPY_AND_ASSIGN(PictureLayerImplTest);
};

TEST_F(PictureLayerImplTest, TileGridAlignment) {
  host_impl_.SetDeviceScaleFactor(1.f);
  TestTileGridAlignmentCommon();
}

TEST_F(PictureLayerImplTest, TileGridAlignmentHiDPI) {
  host_impl_.SetDeviceScaleFactor(2.f);
  TestTileGridAlignmentCommon();
}

TEST_F(PictureLayerImplTest, CloneNoInvalidation) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  Region invalidation;
  AddDefaultTilingsWithInvalidation(invalidation);

  EXPECT_EQ(pending_layer_->tilings().num_tilings(),
            active_layer_->tilings().num_tilings());

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  EXPECT_GT(tilings.num_tilings(), 0u);
  for (size_t i = 0; i < tilings.num_tilings(); ++i)
    VerifyAllTilesExistAndHavePile(tilings.tiling_at(i), active_pile.get());
}

TEST_F(PictureLayerImplTest, ClonePartialInvalidation) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);
  gfx::Rect layer_invalidation(150, 200, 30, 180);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  Region invalidation(layer_invalidation);
  AddDefaultTilingsWithInvalidation(invalidation);

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  EXPECT_GT(tilings.num_tilings(), 0u);
  for (size_t i = 0; i < tilings.num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings.tiling_at(i);
    gfx::Rect content_invalidation = gfx::ToEnclosingRect(gfx::ScaleRect(
        layer_invalidation,
        tiling->contents_scale()));
    for (PictureLayerTiling::Iterator
             iter(tiling,
                  tiling->contents_scale(),
                  tiling->ContentRect());
         iter;
         ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_FALSE(iter.geometry_rect().IsEmpty());
      if (iter.geometry_rect().Intersects(content_invalidation))
        EXPECT_EQ(pending_pile, iter->picture_pile());
      else
        EXPECT_EQ(active_pile, iter->picture_pile());
    }
  }
}

TEST_F(PictureLayerImplTest, CloneFullInvalidation) {
  gfx::Size tile_size(90, 80);
  gfx::Size layer_bounds(300, 500);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  Region invalidation((gfx::Rect(layer_bounds)));
  AddDefaultTilingsWithInvalidation(invalidation);

  EXPECT_EQ(pending_layer_->tilings().num_tilings(),
            active_layer_->tilings().num_tilings());

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  EXPECT_GT(tilings.num_tilings(), 0u);
  for (size_t i = 0; i < tilings.num_tilings(); ++i)
    VerifyAllTilesExistAndHavePile(tilings.tiling_at(i), pending_pile.get());
}

TEST_F(PictureLayerImplTest, NoInvalidationBoundsChange) {
  gfx::Size tile_size(90, 80);
  gfx::Size active_layer_bounds(300, 500);
  gfx::Size pending_layer_bounds(400, 800);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size,
                                                pending_layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, active_layer_bounds);

  SetupTrees(pending_pile, active_pile);

  Region invalidation;
  AddDefaultTilingsWithInvalidation(invalidation);

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  EXPECT_GT(tilings.num_tilings(), 0u);
  for (size_t i = 0; i < tilings.num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings.tiling_at(i);
    gfx::Rect active_content_bounds = gfx::ToEnclosingRect(gfx::ScaleRect(
        gfx::Rect(active_layer_bounds),
        tiling->contents_scale()));
    for (PictureLayerTiling::Iterator
             iter(tiling,
                  tiling->contents_scale(),
                  tiling->ContentRect());
         iter;
         ++iter) {
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

TEST_F(PictureLayerImplTest, AddTilesFromNewRecording) {
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

  SetupTrees(pending_pile, active_pile);
  Region invalidation;
  AddDefaultTilingsWithInvalidation(invalidation);

  const PictureLayerTilingSet& tilings = pending_layer_->tilings();
  EXPECT_GT(tilings.num_tilings(), 0u);
  for (size_t i = 0; i < tilings.num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings.tiling_at(i);

    for (PictureLayerTiling::Iterator
             iter(tiling,
                  tiling->contents_scale(),
                  tiling->ContentRect());
         iter;
         ++iter) {
      EXPECT_FALSE(iter.full_tile_geometry_rect().IsEmpty());
      // Ensure there is a recording for this tile.
      gfx::Rect layer_rect = gfx::ToEnclosingRect(gfx::ScaleRect(
          iter.full_tile_geometry_rect(), 1.f / tiling->contents_scale()));
      layer_rect.Intersect(gfx::Rect(layer_bounds));

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

TEST_F(PictureLayerImplTest, ManageTilingsWithNoRecording) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupTrees(pending_pile, active_pile);

  // These are included in the scale given to the layer.
  host_impl_.SetDeviceScaleFactor(1.f);
  host_impl_.pending_tree()->SetPageScaleFactorAndLimits(1.f, 1.f, 1.f);

  pending_layer_->CalculateContentsScale(
      1.f, false, &result_scale_x, &result_scale_y, &result_bounds);

  EXPECT_EQ(0u, pending_layer_->tilings().num_tilings());
}

TEST_F(PictureLayerImplTest, ManageTilingsCreatesTilings) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, pending_layer_->tilings().num_tilings());

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  // These are included in the scale given to the layer.
  host_impl_.SetDeviceScaleFactor(1.7f);
  host_impl_.pending_tree()->SetPageScaleFactorAndLimits(3.2f, 3.2f, 3.2f);

  pending_layer_->CalculateContentsScale(
      1.3f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings().num_tilings());
  EXPECT_FLOAT_EQ(
      1.3f,
      pending_layer_->tilings().tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.3f * low_res_factor,
      pending_layer_->tilings().tiling_at(1)->contents_scale());

  // If we change the layer's CSS scale factor, then we should not get new
  // tilings.
  pending_layer_->CalculateContentsScale(
      1.8f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings().num_tilings());
  EXPECT_FLOAT_EQ(
      1.3f,
      pending_layer_->tilings().tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.3f * low_res_factor,
      pending_layer_->tilings().tiling_at(1)->contents_scale());

  // If we change the page scale factor, then we should get new tilings.
  host_impl_.pending_tree()->SetPageScaleFactorAndLimits(2.2f, 2.2f, 2.2f);

  pending_layer_->CalculateContentsScale(
      1.8f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(4u, pending_layer_->tilings().num_tilings());
  EXPECT_FLOAT_EQ(
      1.8f,
      pending_layer_->tilings().tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.8f * low_res_factor,
      pending_layer_->tilings().tiling_at(2)->contents_scale());

  // If we change the device scale factor, then we should get new tilings.
  host_impl_.SetDeviceScaleFactor(1.4f);

  pending_layer_->CalculateContentsScale(
      1.9f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(6u, pending_layer_->tilings().num_tilings());
  EXPECT_FLOAT_EQ(
      1.9f,
      pending_layer_->tilings().tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.9f * low_res_factor,
      pending_layer_->tilings().tiling_at(3)->contents_scale());

  // If we change the device scale factor, but end up at the same total scale
  // factor somehow, then we don't get new tilings.
  host_impl_.SetDeviceScaleFactor(2.2f);
  host_impl_.pending_tree()->SetPageScaleFactorAndLimits(1.4f, 1.4f, 1.4f);

  pending_layer_->CalculateContentsScale(
      1.9f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(6u, pending_layer_->tilings().num_tilings());
  EXPECT_FLOAT_EQ(
      1.9f,
      pending_layer_->tilings().tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.9f * low_res_factor,
      pending_layer_->tilings().tiling_at(3)->contents_scale());
}

TEST_F(PictureLayerImplTest, CleanUpTilings) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<TestablePicturePileImpl> pending_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<TestablePicturePileImpl> active_pile =
      TestablePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;
  std::vector<PictureLayerTiling*> used_tilings;

  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, pending_layer_->tilings().num_tilings());

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  // These are included in the scale given to the layer.
  host_impl_.SetDeviceScaleFactor(1.7f);
  host_impl_.pending_tree()->SetPageScaleFactorAndLimits(3.2f, 3.2f, 3.2f);
  host_impl_.active_tree()->SetPageScaleFactorAndLimits(3.2f, 3.2f, 3.2f);

  SetContentsScaleOnBothLayers(1.f, false);
  ASSERT_EQ(2u, active_layer_->tilings().num_tilings());

  // We only have ideal tilings, so they aren't removed.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer_->tilings().num_tilings());

  // Changing the ideal but not creating new tilings.
  SetContentsScaleOnBothLayers(1.5f, false);
  ASSERT_EQ(2u, active_layer_->tilings().num_tilings());

  // The tilings are still our target scale, so they aren't removed.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer_->tilings().num_tilings());

  // Create a 1.2 scale tiling. Now we have 1.0 and 1.2 tilings. Ideal = 1.2.
  host_impl_.pending_tree()->SetPageScaleFactorAndLimits(1.2f, 1.2f, 1.2f);
  host_impl_.active_tree()->SetPageScaleFactorAndLimits(1.2f, 1.2f, 1.2f);
  SetContentsScaleOnBothLayers(1.2f, false);
  ASSERT_EQ(4u, active_layer_->tilings().num_tilings());
  EXPECT_FLOAT_EQ(
      1.f,
      active_layer_->tilings().tiling_at(1)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.f * low_res_factor,
      active_layer_->tilings().tiling_at(3)->contents_scale());

  // Mark the non-ideal tilings as used. They won't be removed.
  used_tilings.clear();
  used_tilings.push_back(active_layer_->tilings().tiling_at(1));
  used_tilings.push_back(active_layer_->tilings().tiling_at(3));
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(4u, active_layer_->tilings().num_tilings());

  // Now move the ideal scale to 0.5. Our target stays 1.2.
  SetContentsScaleOnBothLayers(0.5f, false);

  // All the tilings are between are target and the ideal, so they are not
  // removed.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(4u, active_layer_->tilings().num_tilings());

  // Now move the ideal scale to 1.0. Our target stays 1.2.
  SetContentsScaleOnBothLayers(1.f, false);

  // All the tilings are between are target and the ideal, so they are not
  // removed.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(4u, active_layer_->tilings().num_tilings());

  // Now move the ideal scale to 1.1 on the active layer. Our target stays 1.2.
  active_layer_->CalculateContentsScale(
      1.1f, false, &result_scale_x, &result_scale_y, &result_bounds);

  // Because the pending layer's ideal scale is still 1.0, our tilings fall
  // in the range [1.0,1.2] and are kept.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(4u, active_layer_->tilings().num_tilings());

  // Move the ideal scale on the pending layer to 1.1 as well. Our target stays
  // 1.2 still.
  pending_layer_->CalculateContentsScale(
      1.1f, false, &result_scale_x, &result_scale_y, &result_bounds);

  // Our 1.0 tiling now falls outside the range between our ideal scale and our
  // target raster scale. But it is in our used tilings set, so nothing is
  // deleted.
  used_tilings.clear();
  used_tilings.push_back(active_layer_->tilings().tiling_at(1));
  used_tilings.push_back(active_layer_->tilings().tiling_at(3));
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(4u, active_layer_->tilings().num_tilings());

  // If we remove it from our used tilings set, it is outside the range to keep
  // so it is deleted. Try one tiling at a time.
  used_tilings.clear();
  used_tilings.push_back(active_layer_->tilings().tiling_at(1));
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer_->tilings().num_tilings());
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer_->tilings().num_tilings());
}

}  // namespace
}  // namespace cc
