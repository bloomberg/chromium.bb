// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include <utility>

#include "cc/layers/append_quads_data.h"
#include "cc/layers/picture_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {
namespace {

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

  void SetupDefaultTrees(gfx::Size layer_bounds) {
    gfx::Size tile_size(100, 100);

    scoped_refptr<FakePicturePileImpl> pending_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
    scoped_refptr<FakePicturePileImpl> active_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

    SetupTrees(pending_pile, active_pile);
  }

  void SetupTrees(
      scoped_refptr<PicturePileImpl> pending_pile,
      scoped_refptr<PicturePileImpl> active_pile) {
    SetupPendingTree(active_pile);
    host_impl_.ActivatePendingTree();

    active_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.active_tree()->LayerById(id_));

    SetupPendingTree(pending_pile);
    pending_layer_->UpdateTwinLayer();
  }

  void AddDefaultTilingsWithInvalidation(const Region& invalidation) {
    active_layer_->AddTiling(2.3f);
    active_layer_->AddTiling(1.0f);
    active_layer_->AddTiling(0.5f);
    for (size_t i = 0; i < active_layer_->tilings()->num_tilings(); ++i)
      active_layer_->tilings()->tiling_at(i)->CreateAllTilesForTesting();
    pending_layer_->set_invalidation(invalidation);
    pending_layer_->SyncFromActiveLayer();
    for (size_t i = 0; i < pending_layer_->tilings()->num_tilings(); ++i)
      pending_layer_->tilings()->tiling_at(i)->CreateAllTilesForTesting();
  }

  void SetupPendingTree(
      scoped_refptr<PicturePileImpl> pile) {
    host_impl_.CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl_.pending_tree();
    // Clear recycled tree.
    pending_tree->DetachLayerTree();

    scoped_ptr<FakePictureLayerImpl> pending_layer =
        FakePictureLayerImpl::CreateWithPile(pending_tree, id_, pile);
    pending_layer->SetDrawsContent(true);
    pending_tree->SetRootLayer(pending_layer.PassAs<LayerImpl>());

    pending_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.pending_tree()->LayerById(id_));
  }

  static void VerifyAllTilesExistAndHavePile(
      const PictureLayerTiling* tiling,
      PicturePileImpl* pile) {
    for (PictureLayerTiling::CoverageIterator
             iter(tiling, tiling->contents_scale(), tiling->ContentRect());
         iter;
         ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_EQ(pile, iter->picture_pile());
    }
  }

  void SetContentsScaleOnBothLayers(float contents_scale,
                                    float device_scale_factor,
                                    float page_scale_factor,
                                    bool animating_transform) {
    float result_scale_x, result_scale_y;
    gfx::Size result_bounds;
    pending_layer_->CalculateContentsScale(
        contents_scale,
        device_scale_factor,
        page_scale_factor,
        animating_transform,
        &result_scale_x,
        &result_scale_y,
        &result_bounds);
    active_layer_->CalculateContentsScale(
        contents_scale,
        device_scale_factor,
        page_scale_factor,
        animating_transform,
        &result_scale_x,
        &result_scale_y,
        &result_bounds);
  }

  void ResetTilingsAndRasterScales() {
    pending_layer_->DidLoseOutputSurface();
    active_layer_->DidLoseOutputSurface();
  }

 protected:
  void TestTileGridAlignmentCommon() {
    // Layer to span 4 raster tiles in x and in y
    ImplSidePaintingSettings settings;
    gfx::Size layer_size(
        settings.default_tile_size.width() * 7 / 2,
        settings.default_tile_size.height() * 7 / 2);

    scoped_refptr<FakePicturePileImpl> pending_pile =
        FakePicturePileImpl::CreateFilledPile(layer_size, layer_size);
    scoped_refptr<FakePicturePileImpl> active_pile =
        FakePicturePileImpl::CreateFilledPile(layer_size, layer_size);

    SetupTrees(pending_pile, active_pile);

    float result_scale_x, result_scale_y;
    gfx::Size result_bounds;
    active_layer_->CalculateContentsScale(
        1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);

    // Add 1x1 rects at the centers of each tile, then re-record pile contents
    active_layer_->tilings()->tiling_at(0)->CreateAllTilesForTesting();
    std::vector<Tile*> tiles =
        active_layer_->tilings()->tiling_at(0)->AllTilesForTesting();
    EXPECT_EQ(16u, tiles.size());
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
      active_pile->RasterDirect(
          &mock_canvas, (*tile_iter)->content_rect(), 1.0f, NULL);

      // This test verifies that when drawing the contents of a specific tile
      // at content scale 1.0, the playback canvas never receives content from
      // neighboring tiles which indicates that the tile grid embedded in
      // SkPicture is perfectly aligned with the compositor's tiles.
      EXPECT_EQ(1u, mock_canvas.rects_.size());
      EXPECT_RECT_EQ(*rect_iter, mock_canvas.rects_[0]);
      rect_iter++;
    }
  }

  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  int id_;
  FakePictureLayerImpl* pending_layer_;
  FakePictureLayerImpl* active_layer_;

 private:
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

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  Region invalidation;
  AddDefaultTilingsWithInvalidation(invalidation);

  EXPECT_EQ(pending_layer_->tilings()->num_tilings(),
            active_layer_->tilings()->num_tilings());

  const PictureLayerTilingSet* tilings = pending_layer_->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i)
    VerifyAllTilesExistAndHavePile(tilings->tiling_at(i), active_pile.get());
}

TEST_F(PictureLayerImplTest, ClonePartialInvalidation) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);
  gfx::Rect layer_invalidation(150, 200, 30, 180);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  Region invalidation(layer_invalidation);
  AddDefaultTilingsWithInvalidation(invalidation);

  const PictureLayerTilingSet* tilings = pending_layer_->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings->tiling_at(i);
    gfx::Rect content_invalidation = gfx::ScaleToEnclosingRect(
        layer_invalidation,
        tiling->contents_scale());
    for (PictureLayerTiling::CoverageIterator
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

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  Region invalidation((gfx::Rect(layer_bounds)));
  AddDefaultTilingsWithInvalidation(invalidation);

  EXPECT_EQ(pending_layer_->tilings()->num_tilings(),
            active_layer_->tilings()->num_tilings());

  const PictureLayerTilingSet* tilings = pending_layer_->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i)
    VerifyAllTilesExistAndHavePile(tilings->tiling_at(i), pending_pile.get());
}

TEST_F(PictureLayerImplTest, NoInvalidationBoundsChange) {
  gfx::Size tile_size(90, 80);
  gfx::Size active_layer_bounds(300, 500);
  gfx::Size pending_layer_bounds(400, 800);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size,
                                                pending_layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, active_layer_bounds);

  SetupTrees(pending_pile, active_pile);
  pending_layer_->set_fixed_tile_size(gfx::Size(100, 100));

  Region invalidation;
  AddDefaultTilingsWithInvalidation(invalidation);

  const PictureLayerTilingSet* tilings = pending_layer_->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings->tiling_at(i);
    gfx::Rect active_content_bounds = gfx::ScaleToEnclosingRect(
        gfx::Rect(active_layer_bounds),
        tiling->contents_scale());
    for (PictureLayerTiling::CoverageIterator
             iter(tiling,
                  tiling->contents_scale(),
                  tiling->ContentRect());
         iter;
         ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_FALSE(iter.geometry_rect().IsEmpty());
      std::vector<Tile*> active_tiles =
          active_layer_->tilings()->tiling_at(i)->AllTilesForTesting();
      std::vector<Tile*> pending_tiles = tiling->AllTilesForTesting();
      if (iter.geometry_rect().right() >= active_content_bounds.width() ||
          iter.geometry_rect().bottom() >= active_content_bounds.height() ||
          active_tiles[0]->content_rect().size() !=
              pending_tiles[0]->content_rect().size()) {
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

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);

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

  const PictureLayerTilingSet* tilings = pending_layer_->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings->tiling_at(i);

    for (PictureLayerTiling::CoverageIterator
             iter(tiling,
                  tiling->contents_scale(),
                  tiling->ContentRect());
         iter;
         ++iter) {
      EXPECT_FALSE(iter.full_tile_geometry_rect().IsEmpty());
      // Ensure there is a recording for this tile.
      gfx::Rect layer_rect = gfx::ScaleToEnclosingRect(
          iter.full_tile_geometry_rect(), 1.f / tiling->contents_scale());
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

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupTrees(pending_pile, active_pile);

  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);

  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());
}

TEST_F(PictureLayerImplTest, ManageTilingsCreatesTilings) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  pending_layer_->CalculateContentsScale(1.3f,  // ideal contents scale
                                         1.7f,  // device scale
                                         3.2f,  // page cale
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.3f,
      pending_layer_->tilings()->tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.3f * low_res_factor,
      pending_layer_->tilings()->tiling_at(1)->contents_scale());

  // If we change the layer's CSS scale factor, then we should not get new
  // tilings.
  pending_layer_->CalculateContentsScale(1.8f,  // ideal contents scale
                                         1.7f,  // device scale
                                         3.2f,  // page cale
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.3f,
      pending_layer_->tilings()->tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.3f * low_res_factor,
      pending_layer_->tilings()->tiling_at(1)->contents_scale());

  // If we change the page scale factor, then we should get new tilings.
  pending_layer_->CalculateContentsScale(1.8f,  // ideal contents scale
                                         1.7f,  // device scale
                                         2.2f,  // page cale
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  ASSERT_EQ(4u, pending_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.8f,
      pending_layer_->tilings()->tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.8f * low_res_factor,
      pending_layer_->tilings()->tiling_at(2)->contents_scale());

  // If we change the device scale factor, then we should get new tilings.
  pending_layer_->CalculateContentsScale(1.9f,  // ideal contents scale
                                         1.4f,  // device scale
                                         2.2f,  // page cale
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  ASSERT_EQ(6u, pending_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.9f,
      pending_layer_->tilings()->tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.9f * low_res_factor,
      pending_layer_->tilings()->tiling_at(3)->contents_scale());

  // If we change the device scale factor, but end up at the same total scale
  // factor somehow, then we don't get new tilings.
  pending_layer_->CalculateContentsScale(1.9f,  // ideal contents scale
                                         2.2f,  // device scale
                                         1.4f,  // page cale
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  ASSERT_EQ(6u, pending_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.9f,
      pending_layer_->tilings()->tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.9f * low_res_factor,
      pending_layer_->tilings()->tiling_at(3)->contents_scale());
}

TEST_F(PictureLayerImplTest, CleanUpTilings) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;
  std::vector<PictureLayerTiling*> used_tilings;

  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  float device_scale = 1.7f;
  float page_scale = 3.2f;

  SetContentsScaleOnBothLayers(1.f, device_scale, page_scale, false);
  ASSERT_EQ(2u, active_layer_->tilings()->num_tilings());

  // We only have ideal tilings, so they aren't removed.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer_->tilings()->num_tilings());

  // Changing the ideal but not creating new tilings.
  SetContentsScaleOnBothLayers(1.5f, device_scale, page_scale, false);
  ASSERT_EQ(2u, active_layer_->tilings()->num_tilings());

  // The tilings are still our target scale, so they aren't removed.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer_->tilings()->num_tilings());

  // Create a 1.2 scale tiling. Now we have 1.0 and 1.2 tilings. Ideal = 1.2.
  page_scale = 1.2f;
  SetContentsScaleOnBothLayers(1.2f, device_scale, page_scale, false);
  ASSERT_EQ(4u, active_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.f,
      active_layer_->tilings()->tiling_at(1)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.f * low_res_factor,
      active_layer_->tilings()->tiling_at(3)->contents_scale());

  // Mark the non-ideal tilings as used. They won't be removed.
  used_tilings.clear();
  used_tilings.push_back(active_layer_->tilings()->tiling_at(1));
  used_tilings.push_back(active_layer_->tilings()->tiling_at(3));
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(4u, active_layer_->tilings()->num_tilings());

  // Now move the ideal scale to 0.5. Our target stays 1.2.
  SetContentsScaleOnBothLayers(0.5f, device_scale, page_scale, false);

  // The high resolution tiling is between target and ideal, so is not
  // removed.  The low res tiling for the old ideal=1.0 scale is removed.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer_->tilings()->num_tilings());

  // Now move the ideal scale to 1.0. Our target stays 1.2.
  SetContentsScaleOnBothLayers(1.f, device_scale, page_scale, false);

  // All the tilings are between are target and the ideal, so they are not
  // removed.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer_->tilings()->num_tilings());

  // Now move the ideal scale to 1.1 on the active layer. Our target stays 1.2.
  active_layer_->CalculateContentsScale(1.1f,
                                        device_scale,
                                        page_scale,
                                        false,
                                        &result_scale_x,
                                        &result_scale_y,
                                        &result_bounds);

  // Because the pending layer's ideal scale is still 1.0, our tilings fall
  // in the range [1.0,1.2] and are kept.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer_->tilings()->num_tilings());

  // Move the ideal scale on the pending layer to 1.1 as well. Our target stays
  // 1.2 still.
  pending_layer_->CalculateContentsScale(1.1f,
                                         device_scale,
                                         page_scale,
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);

  // Our 1.0 tiling now falls outside the range between our ideal scale and our
  // target raster scale. But it is in our used tilings set, so nothing is
  // deleted.
  used_tilings.clear();
  used_tilings.push_back(active_layer_->tilings()->tiling_at(1));
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer_->tilings()->num_tilings());

  // If we remove it from our used tilings set, it is outside the range to keep
  // so it is deleted.
  used_tilings.clear();
  active_layer_->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer_->tilings()->num_tilings());
}

#define EXPECT_BOTH_EQ(expression, x)         \
  do {                                        \
    EXPECT_EQ(pending_layer_->expression, x); \
    EXPECT_EQ(active_layer_->expression, x);  \
  } while (false)

TEST_F(PictureLayerImplTest, DontAddLowResDuringAnimation) {
  SetupDefaultTrees(gfx::Size(300, 100));

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  bool animating_transform = true;

  // Animating, so don't create low res even if there isn't one already.
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 1.f);
  EXPECT_BOTH_EQ(num_tilings(), 1u);

  // Stop animating, low res gets created.
  animating_transform = false;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 1.f);
  EXPECT_BOTH_EQ(LowResTiling()->contents_scale(), low_res_factor);
  EXPECT_BOTH_EQ(num_tilings(), 2u);

  // Page scale animation, new high res, but not new low res because animating.
  contents_scale = 4.f;
  page_scale = 4.f;
  animating_transform = true;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 4.f);
  EXPECT_BOTH_EQ(LowResTiling()->contents_scale(), low_res_factor);
  EXPECT_BOTH_EQ(num_tilings(), 3u);

  // Stop animating, new low res gets created for final page scale.
  animating_transform = false;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 4.f);
  EXPECT_BOTH_EQ(LowResTiling()->contents_scale(), 4.f * low_res_factor);
  EXPECT_BOTH_EQ(num_tilings(), 4u);
}

TEST_F(PictureLayerImplTest, DontAddLowResForSmallLayers) {
  gfx::Size tile_size(host_impl_.settings().default_tile_size);
  SetupDefaultTrees(tile_size);

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  bool animating_transform = false;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), contents_scale);
  EXPECT_BOTH_EQ(num_tilings(), 1u);

  ResetTilingsAndRasterScales();

  // Any content bounds that would create more than one tile will
  // generate a low res tiling.
  contents_scale = 1.2f;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), contents_scale);
  EXPECT_BOTH_EQ(LowResTiling()->contents_scale(),
                 contents_scale * low_res_factor);
  EXPECT_BOTH_EQ(num_tilings(), 2u);

  ResetTilingsAndRasterScales();

  // Mask layers dont create low res since they always fit on one tile.
  pending_layer_->SetIsMask(true);
  active_layer_->SetIsMask(true);
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), contents_scale);
  EXPECT_BOTH_EQ(num_tilings(), 1u);
}

TEST_F(PictureLayerImplTest, DidLoseOutputSurface) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());

  pending_layer_->CalculateContentsScale(1.3f,  // ideal contents scale
                                         2.7f,  // device scale
                                         3.2f,  // page cale
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  EXPECT_EQ(2u, pending_layer_->tilings()->num_tilings());

  // All tilings should be removed when losing output surface.
  active_layer_->DidLoseOutputSurface();
  EXPECT_EQ(0u, active_layer_->tilings()->num_tilings());
  pending_layer_->DidLoseOutputSurface();
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());

  // This should create new tilings.
  pending_layer_->CalculateContentsScale(1.3f,  // ideal contents scale
                                         2.7f,  // device scale
                                         3.2f,  // page cale
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  EXPECT_EQ(2u, pending_layer_->tilings()->num_tilings());
}

TEST_F(PictureLayerImplTest, ClampTilesToToMaxTileSize) {
  // The default max tile size is larger than 400x400.
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(5000, 5000);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());

  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());

  pending_layer_->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // The default value.
  EXPECT_EQ(gfx::Size(256, 256).ToString(),
            host_impl_.settings().default_tile_size.ToString());

  Tile* tile = pending_layer_->tilings()->tiling_at(0)->AllTilesForTesting()[0];
  EXPECT_EQ(gfx::Size(256, 256).ToString(),
            tile->content_rect().size().ToString());

  pending_layer_->DidLoseOutputSurface();

  // Change the max texture size on the output surface context.
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  context->set_max_texture_size(140);
  host_impl_.InitializeRenderer(FakeOutputSurface::Create3d(
      context.PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>());

  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());

  pending_layer_->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // Verify the tiles are not larger than the context's max texture size.
  tile = pending_layer_->tilings()->tiling_at(0)->AllTilesForTesting()[0];
  EXPECT_GE(140, tile->content_rect().width());
  EXPECT_GE(140, tile->content_rect().height());
}

TEST_F(PictureLayerImplTest, ClampSingleTileToToMaxTileSize) {
  // The default max tile size is larger than 400x400.
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(500, 500);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());

  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());

  pending_layer_->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // The default value. The layer is smaller than this.
  EXPECT_EQ(gfx::Size(512, 512).ToString(),
            host_impl_.settings().max_untiled_layer_size.ToString());

  // There should be a single tile since the layer is small.
  PictureLayerTiling* high_res_tiling = pending_layer_->tilings()->tiling_at(0);
  EXPECT_EQ(1u, high_res_tiling->AllTilesForTesting().size());

  pending_layer_->DidLoseOutputSurface();

  // Change the max texture size on the output surface context.
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  context->set_max_texture_size(140);
  host_impl_.InitializeRenderer(FakeOutputSurface::Create3d(
      context.PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>());

  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());

  pending_layer_->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // There should be more than one tile since the max texture size won't cover
  // the layer.
  high_res_tiling = pending_layer_->tilings()->tiling_at(0);
  EXPECT_LT(1u, high_res_tiling->AllTilesForTesting().size());

  // Verify the tiles are not larger than the context's max texture size.
  Tile* tile = pending_layer_->tilings()->tiling_at(0)->AllTilesForTesting()[0];
  EXPECT_GE(140, tile->content_rect().width());
  EXPECT_GE(140, tile->content_rect().height());
}

TEST_F(PictureLayerImplTest, DisallowTileDrawQuads) {
  MockQuadCuller quad_culler;

  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  active_layer_->SetContentBounds(layer_bounds);
  active_layer_->draw_properties().visible_content_rect =
      gfx::Rect(layer_bounds);

  gfx::Rect layer_invalidation(150, 200, 30, 180);
  Region invalidation(layer_invalidation);
  AddDefaultTilingsWithInvalidation(invalidation);

  AppendQuadsData data;
  active_layer_->WillDraw(DRAW_MODE_RESOURCELESS_SOFTWARE, NULL);
  active_layer_->AppendQuads(&quad_culler, &data);
  active_layer_->DidDraw(NULL);

  ASSERT_EQ(1U, quad_culler.quad_list().size());
  EXPECT_EQ(DrawQuad::PICTURE_CONTENT, quad_culler.quad_list()[0]->material);
}

TEST_F(PictureLayerImplTest, MarkRequiredNullTiles) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);
  // Layers with entirely empty piles can't get tilings.
  pending_pile->AddRecordingAt(0, 0);

  SetupPendingTree(pending_pile);

  ASSERT_TRUE(pending_layer_->CanHaveTilings());
  pending_layer_->AddTiling(1.0f);
  pending_layer_->AddTiling(2.0f);

  // It should be safe to call this (and MarkVisibleResourcesAsRequired)
  // on a layer with no recordings.
  host_impl_.pending_tree()->UpdateDrawProperties();
  pending_layer_->MarkVisibleResourcesAsRequired();
}

TEST_F(PictureLayerImplTest, MarkRequiredOffscreenTiles) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(200, 100);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  SetupPendingTree(pending_pile);

  pending_layer_->set_fixed_tile_size(tile_size);
  ASSERT_TRUE(pending_layer_->CanHaveTilings());
  PictureLayerTiling* tiling = pending_layer_->AddTiling(1.f);
  host_impl_.pending_tree()->UpdateDrawProperties();
  EXPECT_EQ(tiling->resolution(), HIGH_RESOLUTION);

  // Fake set priorities.
  int tile_count = 0;
  for (PictureLayerTiling::CoverageIterator iter(
           tiling,
           pending_layer_->contents_scale_x(),
           gfx::Rect(pending_layer_->visible_content_rect()));
       iter;
       ++iter) {
    if (!*iter)
      continue;
    Tile* tile = *iter;
    TilePriority priority;
    priority.resolution = HIGH_RESOLUTION;
    if (++tile_count % 2) {
      priority.time_to_visible_in_seconds = 0.f;
      priority.distance_to_visible_in_pixels = 0.f;
    } else {
      priority.time_to_visible_in_seconds = 1.f;
      priority.distance_to_visible_in_pixels = 1.f;
    }
    tile->SetPriority(PENDING_TREE, priority);
  }

  pending_layer_->MarkVisibleResourcesAsRequired();

  int num_visible = 0;
  int num_offscreen = 0;

  for (PictureLayerTiling::CoverageIterator iter(
           tiling,
           pending_layer_->contents_scale_x(),
           gfx::Rect(pending_layer_->visible_content_rect()));
       iter;
       ++iter) {
    if (!*iter)
      continue;
    const Tile* tile = *iter;
    if (tile->priority(PENDING_TREE).distance_to_visible_in_pixels == 0.f) {
      EXPECT_TRUE(tile->required_for_activation());
      num_visible++;
    } else {
      EXPECT_FALSE(tile->required_for_activation());
      num_offscreen++;
    }
  }

  EXPECT_GT(num_visible, 0);
  EXPECT_GT(num_offscreen, 0);
}

}  // namespace
}  // namespace cc
