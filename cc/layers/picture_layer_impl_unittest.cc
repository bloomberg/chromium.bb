// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include <algorithm>
#include <limits>
#include <set>
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
#include "cc/test/gpu_rasterization_settings.h"
#include "cc/test/hybrid_rasterization_settings.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {
namespace {

class MockCanvas : public SkCanvas {
 public:
  explicit MockCanvas(int w, int h) : SkCanvas(w, h) {}

  virtual void drawRect(const SkRect& rect, const SkPaint& paint) OVERRIDE {
    // Capture calls before SkCanvas quickReject() kicks in.
    rects_.push_back(rect);
  }

  std::vector<SkRect> rects_;
};

class PictureLayerImplTest : public testing::Test {
 public:
  PictureLayerImplTest()
      : proxy_(base::MessageLoopProxy::current()),
        host_impl_(ImplSidePaintingSettings(),
                   &proxy_,
                   &shared_bitmap_manager_),
        id_(7) {}

  explicit PictureLayerImplTest(const LayerTreeSettings& settings)
      : proxy_(base::MessageLoopProxy::current()),
        host_impl_(settings, &proxy_, &shared_bitmap_manager_),
        id_(7) {}

  virtual ~PictureLayerImplTest() {
  }

  virtual void SetUp() OVERRIDE {
    InitializeRenderer();
  }

  virtual void InitializeRenderer() {
    host_impl_.InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());
  }

  void SetupDefaultTrees(const gfx::Size& layer_bounds) {
    gfx::Size tile_size(100, 100);

    scoped_refptr<FakePicturePileImpl> pending_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
    scoped_refptr<FakePicturePileImpl> active_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

    SetupTrees(pending_pile, active_pile);
  }

  void ActivateTree() {
    host_impl_.ActivatePendingTree();
    CHECK(!host_impl_.pending_tree());
    pending_layer_ = NULL;
    active_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.active_tree()->LayerById(id_));
  }

  void SetupDefaultTreesWithFixedTileSize(const gfx::Size& layer_bounds,
                                          const gfx::Size& tile_size) {
    SetupDefaultTrees(layer_bounds);
    pending_layer_->set_fixed_tile_size(tile_size);
    active_layer_->set_fixed_tile_size(tile_size);
  }

  void SetupTrees(
      scoped_refptr<PicturePileImpl> pending_pile,
      scoped_refptr<PicturePileImpl> active_pile) {
    SetupPendingTree(active_pile);
    ActivateTree();
    SetupPendingTree(pending_pile);
  }

  void CreateHighLowResAndSetAllTilesVisible() {
    // Active layer must get updated first so pending layer can share from it.
    active_layer_->CreateDefaultTilingsAndTiles();
    active_layer_->SetAllTilesVisible();
    pending_layer_->CreateDefaultTilingsAndTiles();
    pending_layer_->SetAllTilesVisible();
  }

  void AddDefaultTilingsWithInvalidation(const Region& invalidation) {
    active_layer_->AddTiling(2.3f);
    active_layer_->AddTiling(1.0f);
    active_layer_->AddTiling(0.5f);
    for (size_t i = 0; i < active_layer_->tilings()->num_tilings(); ++i)
      active_layer_->tilings()->tiling_at(i)->CreateAllTilesForTesting();
    pending_layer_->set_invalidation(invalidation);
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
    pending_layer_->DoPostCommitInitializationIfNeeded();
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
    pending_layer_->ReleaseResources();
    active_layer_->ReleaseResources();
  }

  void AssertAllTilesRequired(PictureLayerTiling* tiling) {
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    for (size_t i = 0; i < tiles.size(); ++i)
      EXPECT_TRUE(tiles[i]->required_for_activation()) << "i: " << i;
    EXPECT_GT(tiles.size(), 0u);
  }

  void AssertNoTilesRequired(PictureLayerTiling* tiling) {
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    for (size_t i = 0; i < tiles.size(); ++i)
      EXPECT_FALSE(tiles[i]->required_for_activation()) << "i: " << i;
    EXPECT_GT(tiles.size(), 0u);
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

    std::vector<SkRect>::const_iterator rect_iter = rects.begin();
    for (tile_iter = tiles.begin(); tile_iter < tiles.end(); tile_iter++) {
      MockCanvas mock_canvas(1000, 1000);
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
  TestSharedBitmapManager shared_bitmap_manager_;
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

TEST_F(PictureLayerImplTest, TileManagerRegisterUnregister) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  std::vector<TileManager::PairedPictureLayer> paired_layers;
  host_impl_.tile_manager()->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(0u, paired_layers.size());

  // Update tile priorities will force the layer to register itself.
  float dummy_contents_scale_x;
  float dummy_contents_scale_y;
  gfx::Size dummy_content_bounds;
  active_layer_->CalculateContentsScale(1.f,
                                        1.f,
                                        1.f,
                                        false,
                                        &dummy_contents_scale_x,
                                        &dummy_contents_scale_y,
                                        &dummy_content_bounds);
  active_layer_->UpdateTilePriorities();
  host_impl_.pending_tree()->UpdateDrawProperties();
  pending_layer_->CalculateContentsScale(1.f,
                                         1.f,
                                         1.f,
                                         false,
                                         &dummy_contents_scale_x,
                                         &dummy_contents_scale_y,
                                         &dummy_content_bounds);
  pending_layer_->UpdateTilePriorities();

  host_impl_.tile_manager()->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(1u, paired_layers.size());
  EXPECT_EQ(active_layer_, paired_layers[0].active_layer);
  EXPECT_EQ(pending_layer_, paired_layers[0].pending_layer);

  // Destroy and recreate tile manager.
  host_impl_.DidLoseOutputSurface();
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  host_impl_.InitializeRenderer(
      FakeOutputSurface::Create3d(context.Pass()).PassAs<OutputSurface>());

  host_impl_.tile_manager()->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(0u, paired_layers.size());

  active_layer_->CalculateContentsScale(1.f,
                                        1.f,
                                        1.f,
                                        false,
                                        &dummy_contents_scale_x,
                                        &dummy_contents_scale_y,
                                        &dummy_content_bounds);
  active_layer_->UpdateTilePriorities();
  host_impl_.pending_tree()->UpdateDrawProperties();
  pending_layer_->CalculateContentsScale(1.f,
                                         1.f,
                                         1.f,
                                         false,
                                         &dummy_contents_scale_x,
                                         &dummy_contents_scale_y,
                                         &dummy_content_bounds);
  pending_layer_->UpdateTilePriorities();

  host_impl_.tile_manager()->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(1u, paired_layers.size());
  EXPECT_EQ(active_layer_, paired_layers[0].active_layer);
  EXPECT_EQ(pending_layer_, paired_layers[0].pending_layer);
}

TEST_F(PictureLayerImplTest, SuppressUpdateTilePriorities) {
  base::TimeTicks time_ticks;
  host_impl_.SetCurrentFrameTimeTicks(time_ticks);

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);

  Region invalidation;
  AddDefaultTilingsWithInvalidation(invalidation);
  float dummy_contents_scale_x;
  float dummy_contents_scale_y;
  gfx::Size dummy_content_bounds;
  active_layer_->CalculateContentsScale(1.f,
                                        1.f,
                                        1.f,
                                        false,
                                        &dummy_contents_scale_x,
                                        &dummy_contents_scale_y,
                                        &dummy_content_bounds);

  EXPECT_TRUE(host_impl_.manage_tiles_needed());
  active_layer_->UpdateTilePriorities();
  host_impl_.ManageTiles();
  EXPECT_FALSE(host_impl_.manage_tiles_needed());

  time_ticks += base::TimeDelta::FromMilliseconds(200);
  host_impl_.SetCurrentFrameTimeTicks(time_ticks);

  // Setting this boolean should cause an early out in UpdateTilePriorities.
  bool valid_for_tile_management = false;
  host_impl_.SetExternalDrawConstraints(gfx::Transform(),
                                        gfx::Rect(layer_bounds),
                                        gfx::Rect(layer_bounds),
                                        valid_for_tile_management);
  active_layer_->UpdateTilePriorities();
  EXPECT_FALSE(host_impl_.manage_tiles_needed());

  time_ticks += base::TimeDelta::FromMilliseconds(200);
  host_impl_.SetCurrentFrameTimeTicks(time_ticks);

  valid_for_tile_management = true;
  host_impl_.SetExternalDrawConstraints(gfx::Transform(),
                                        gfx::Rect(layer_bounds),
                                        gfx::Rect(layer_bounds),
                                        valid_for_tile_management);
  active_layer_->UpdateTilePriorities();
  EXPECT_TRUE(host_impl_.manage_tiles_needed());
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
      bool in_pending = pending_pile->CanRaster(tiling->contents_scale(),
                                                iter.full_tile_geometry_rect());
      bool in_active = active_pile->CanRaster(tiling->contents_scale(),
                                              iter.full_tile_geometry_rect());

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

TEST_F(PictureLayerImplTest, CreateTilingsEvenIfTwinHasNone) {
  // This test makes sure that if a layer can have tilings, then a commit makes
  // it not able to have tilings (empty size), and then a future commit that
  // makes it valid again should be able to create tilings.
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<FakePicturePileImpl> empty_pile =
      FakePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> valid_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  float high_res_scale = 1.3f;
  float low_res_scale = high_res_scale * low_res_factor;
  float device_scale = 1.7f;
  float page_scale = 3.2f;
  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupPendingTree(valid_pile);
  pending_layer_->CalculateContentsScale(high_res_scale,
                                         device_scale,
                                         page_scale,
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(high_res_scale,
                  pending_layer_->HighResTiling()->contents_scale());
  EXPECT_FLOAT_EQ(low_res_scale,
                  pending_layer_->LowResTiling()->contents_scale());

  ActivateTree();
  SetupPendingTree(empty_pile);
  pending_layer_->CalculateContentsScale(high_res_scale,
                                         device_scale,
                                         page_scale,
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  ASSERT_EQ(2u, active_layer_->tilings()->num_tilings());
  ASSERT_EQ(0u, pending_layer_->tilings()->num_tilings());

  ActivateTree();
  active_layer_->CalculateContentsScale(high_res_scale,
                                        device_scale,
                                        page_scale,
                                        false,
                                        &result_scale_x,
                                        &result_scale_y,
                                        &result_bounds);
  ASSERT_EQ(0u, active_layer_->tilings()->num_tilings());

  SetupPendingTree(valid_pile);
  pending_layer_->CalculateContentsScale(high_res_scale,
                                         device_scale,
                                         page_scale,
                                         false,
                                         &result_scale_x,
                                         &result_scale_y,
                                         &result_bounds);
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());
  ASSERT_EQ(0u, active_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(high_res_scale,
                  pending_layer_->HighResTiling()->contents_scale());
  EXPECT_FLOAT_EQ(low_res_scale,
                  pending_layer_->LowResTiling()->contents_scale());
}

TEST_F(PictureLayerImplTest, ZoomOutCrash) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  // Set up the high and low res tilings before pinch zoom.
  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, active_layer_->tilings()->num_tilings());
  SetContentsScaleOnBothLayers(32.0f, 1.0f, 32.0f, false);
  host_impl_.PinchGestureBegin();
  SetContentsScaleOnBothLayers(1.0f, 1.0f, 1.0f, false);
  SetContentsScaleOnBothLayers(1.0f, 1.0f, 1.0f, false);
  EXPECT_EQ(active_layer_->tilings()->NumHighResTilings(), 1);
}

TEST_F(PictureLayerImplTest, PinchGestureTilings) {
  gfx::Size tile_size(400, 400);
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  // Set up the high and low res tilings before pinch zoom.
  SetupTrees(pending_pile, active_pile);
  EXPECT_EQ(0u, active_layer_->tilings()->num_tilings());
  SetContentsScaleOnBothLayers(1.0f, 1.0f, 1.0f, false);
  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  EXPECT_EQ(2u, active_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.0f,
      active_layer_->tilings()->tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.0f * low_res_factor,
      active_layer_->tilings()->tiling_at(1)->contents_scale());

  // Start a pinch gesture.
  host_impl_.PinchGestureBegin();

  // Zoom out by a small amount. We should create a tiling at half
  // the scale (1/kMaxScaleRatioDuringPinch).
  SetContentsScaleOnBothLayers(0.90f, 1.0f, 0.9f, false);
  EXPECT_EQ(3u, active_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.0f,
      active_layer_->tilings()->tiling_at(0)->contents_scale());
  EXPECT_FLOAT_EQ(
      0.5f,
      active_layer_->tilings()->tiling_at(1)->contents_scale());
  EXPECT_FLOAT_EQ(
      1.0f * low_res_factor,
      active_layer_->tilings()->tiling_at(2)->contents_scale());

  // Zoom out further, close to our low-res scale factor. We should
  // use that tiling as high-res, and not create a new tiling.
  SetContentsScaleOnBothLayers(low_res_factor, 1.0f, low_res_factor, false);
  EXPECT_EQ(3u, active_layer_->tilings()->num_tilings());

  // Zoom in a lot now. Since we increase by increments of
  // kMaxScaleRatioDuringPinch, this will first use 0.5, then 1.0
  // and then finally create a new tiling at 2.0.
  SetContentsScaleOnBothLayers(2.1f, 1.0f, 2.1f, false);
  EXPECT_EQ(3u, active_layer_->tilings()->num_tilings());
  SetContentsScaleOnBothLayers(2.1f, 1.0f, 2.1f, false);
  EXPECT_EQ(3u, active_layer_->tilings()->num_tilings());
  SetContentsScaleOnBothLayers(2.1f, 1.0f, 2.1f, false);
  EXPECT_EQ(4u, active_layer_->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      2.0f,
      active_layer_->tilings()->tiling_at(0)->contents_scale());
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
  // Make sure this layer covers multiple tiles, since otherwise low
  // res won't get created because it is too small.
  gfx::Size tile_size(host_impl_.settings().default_tile_size);
  SetupDefaultTrees(gfx::Size(tile_size.width() + 1, tile_size.height() + 1));
  // Avoid max untiled layer size heuristics via fixed tile size.
  pending_layer_->set_fixed_tile_size(tile_size);
  active_layer_->set_fixed_tile_size(tile_size);

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
  contents_scale = 2.f;
  page_scale = 2.f;
  animating_transform = true;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 2.f);
  EXPECT_BOTH_EQ(LowResTiling()->contents_scale(), low_res_factor);
  EXPECT_BOTH_EQ(num_tilings(), 3u);

  // Stop animating, new low res gets created for final page scale.
  animating_transform = false;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 2.f);
  EXPECT_BOTH_EQ(LowResTiling()->contents_scale(), 2.f * low_res_factor);
  EXPECT_BOTH_EQ(num_tilings(), 4u);
}

TEST_F(PictureLayerImplTest, DontAddLowResForSmallLayers) {
  gfx::Size tile_size(host_impl_.settings().default_tile_size);
  SetupDefaultTrees(tile_size);

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;
  float device_scale = 1.f;
  float page_scale = 1.f;
  bool animating_transform = false;

  // Contents exactly fit on one tile at scale 1, no low res.
  float contents_scale = 1.f;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), contents_scale);
  EXPECT_BOTH_EQ(num_tilings(), 1u);

  ResetTilingsAndRasterScales();

  // Contents that are smaller than one tile, no low res.
  contents_scale = 0.123f;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), contents_scale);
  EXPECT_BOTH_EQ(num_tilings(), 1u);

  ResetTilingsAndRasterScales();

  // Any content bounds that would create more than one tile will
  // generate a low res tiling.
  contents_scale = 2.5f;
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

TEST_F(PictureLayerImplTest, ReleaseResources) {
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
  active_layer_->ReleaseResources();
  EXPECT_EQ(0u, active_layer_->tilings()->num_tilings());
  pending_layer_->ReleaseResources();
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

  pending_layer_->ReleaseResources();

  // Change the max texture size on the output surface context.
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  context->set_max_texture_size(140);
  host_impl_.DidLoseOutputSurface();
  host_impl_.InitializeRenderer(FakeOutputSurface::Create3d(
      context.Pass()).PassAs<OutputSurface>());

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
  ASSERT_LE(1u, pending_layer_->tilings()->num_tilings());

  pending_layer_->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // The default value. The layer is smaller than this.
  EXPECT_EQ(gfx::Size(512, 512).ToString(),
            host_impl_.settings().max_untiled_layer_size.ToString());

  // There should be a single tile since the layer is small.
  PictureLayerTiling* high_res_tiling = pending_layer_->tilings()->tiling_at(0);
  EXPECT_EQ(1u, high_res_tiling->AllTilesForTesting().size());

  pending_layer_->ReleaseResources();

  // Change the max texture size on the output surface context.
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  context->set_max_texture_size(140);
  host_impl_.DidLoseOutputSurface();
  host_impl_.InitializeRenderer(FakeOutputSurface::Create3d(
      context.Pass()).PassAs<OutputSurface>());

  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  ASSERT_LE(1u, pending_layer_->tilings()->num_tilings());

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
  gfx::Size layer_bounds(200, 200);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  SetupPendingTree(pending_pile);

  pending_layer_->set_fixed_tile_size(tile_size);
  ASSERT_TRUE(pending_layer_->CanHaveTilings());
  PictureLayerTiling* tiling = pending_layer_->AddTiling(1.f);
  host_impl_.pending_tree()->UpdateDrawProperties();
  EXPECT_EQ(tiling->resolution(), HIGH_RESOLUTION);

  pending_layer_->draw_properties().visible_content_rect =
      gfx::Rect(0, 0, 100, 200);

  // Fake set priorities.
  for (PictureLayerTiling::CoverageIterator iter(
           tiling, pending_layer_->contents_scale_x(), gfx::Rect(layer_bounds));
       iter;
       ++iter) {
    if (!*iter)
      continue;
    Tile* tile = *iter;
    TilePriority priority;
    priority.resolution = HIGH_RESOLUTION;
    gfx::Rect tile_bounds = iter.geometry_rect();
    if (pending_layer_->visible_content_rect().Intersects(tile_bounds)) {
      priority.priority_bin = TilePriority::NOW;
      priority.distance_to_visible = 0.f;
    } else {
      priority.priority_bin = TilePriority::SOON;
      priority.distance_to_visible = 1.f;
    }
    tile->SetPriority(PENDING_TREE, priority);
  }

  pending_layer_->MarkVisibleResourcesAsRequired();

  int num_visible = 0;
  int num_offscreen = 0;

  for (PictureLayerTiling::CoverageIterator iter(
           tiling, pending_layer_->contents_scale_x(), gfx::Rect(layer_bounds));
       iter;
       ++iter) {
    if (!*iter)
      continue;
    const Tile* tile = *iter;
    if (tile->priority(PENDING_TREE).distance_to_visible == 0.f) {
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

TEST_F(PictureLayerImplTest, HighResRequiredWhenUnsharedActiveAllReady) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);
  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size);

  // No tiles shared.
  pending_layer_->set_invalidation(gfx::Rect(layer_bounds));

  CreateHighLowResAndSetAllTilesVisible();

  active_layer_->SetAllTilesReady();

  // No shared tiles and all active tiles ready, so pending can only
  // activate with all high res tiles.
  pending_layer_->MarkVisibleResourcesAsRequired();
  AssertAllTilesRequired(pending_layer_->HighResTiling());
  AssertNoTilesRequired(pending_layer_->LowResTiling());
}

TEST_F(PictureLayerImplTest, HighResRequiredWhenMissingHighResFlagOn) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);
  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size);

  // All tiles shared (no invalidation).
  CreateHighLowResAndSetAllTilesVisible();

  // Verify active tree not ready.
  Tile* some_active_tile =
      active_layer_->HighResTiling()->AllTilesForTesting()[0];
  EXPECT_FALSE(some_active_tile->IsReadyToDraw());

  // When high res are required, even if the active tree is not ready,
  // the high res tiles must be ready.
  host_impl_.active_tree()->SetRequiresHighResToDraw();
  pending_layer_->MarkVisibleResourcesAsRequired();
  AssertAllTilesRequired(pending_layer_->HighResTiling());
  AssertNoTilesRequired(pending_layer_->LowResTiling());
}

TEST_F(PictureLayerImplTest, NothingRequiredIfAllHighResTilesShared) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);
  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size);

  CreateHighLowResAndSetAllTilesVisible();

  Tile* some_active_tile =
      active_layer_->HighResTiling()->AllTilesForTesting()[0];
  EXPECT_FALSE(some_active_tile->IsReadyToDraw());

  // All tiles shared (no invalidation), so even though the active tree's
  // tiles aren't ready, there is nothing required.
  pending_layer_->MarkVisibleResourcesAsRequired();
  AssertNoTilesRequired(pending_layer_->HighResTiling());
  AssertNoTilesRequired(pending_layer_->LowResTiling());
}

TEST_F(PictureLayerImplTest, NothingRequiredIfActiveMissingTiles) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);
  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  // This pile will create tilings, but has no recordings so will not create any
  // tiles.  This is attempting to simulate scrolling past the end of recorded
  // content on the active layer, where the recordings are so far away that
  // no tiles are created.
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateEmptyPileThatThinksItHasRecordings(
          tile_size, layer_bounds);
  SetupTrees(pending_pile, active_pile);
  pending_layer_->set_fixed_tile_size(tile_size);
  active_layer_->set_fixed_tile_size(tile_size);

  CreateHighLowResAndSetAllTilesVisible();

  // Active layer has tilings, but no tiles due to missing recordings.
  EXPECT_TRUE(active_layer_->CanHaveTilings());
  EXPECT_EQ(active_layer_->tilings()->num_tilings(), 2u);
  EXPECT_EQ(active_layer_->HighResTiling()->AllTilesForTesting().size(), 0u);

  // Since the active layer has no tiles at all, the pending layer doesn't
  // need content in order to activate.
  pending_layer_->MarkVisibleResourcesAsRequired();
  AssertNoTilesRequired(pending_layer_->HighResTiling());
  AssertNoTilesRequired(pending_layer_->LowResTiling());
}

TEST_F(PictureLayerImplTest, HighResRequiredIfActiveCantHaveTiles) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);
  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_refptr<FakePicturePileImpl> active_pile =
      FakePicturePileImpl::CreateEmptyPile(tile_size, layer_bounds);
  SetupTrees(pending_pile, active_pile);
  pending_layer_->set_fixed_tile_size(tile_size);
  active_layer_->set_fixed_tile_size(tile_size);

  CreateHighLowResAndSetAllTilesVisible();

  // Active layer can't have tiles.
  EXPECT_FALSE(active_layer_->CanHaveTilings());

  // All high res tiles required.  This should be considered identical
  // to the case where there is no active layer, to avoid flashing content.
  // This can happen if a layer exists for a while and switches from
  // not being able to have content to having content.
  pending_layer_->MarkVisibleResourcesAsRequired();
  AssertAllTilesRequired(pending_layer_->HighResTiling());
  AssertNoTilesRequired(pending_layer_->LowResTiling());
}

TEST_F(PictureLayerImplTest, ActivateUninitializedLayer) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);
  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  host_impl_.CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl_.pending_tree();

  scoped_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::CreateWithPile(pending_tree, id_, pending_pile);
  pending_layer->SetDrawsContent(true);
  pending_tree->SetRootLayer(pending_layer.PassAs<LayerImpl>());

  pending_layer_ = static_cast<FakePictureLayerImpl*>(
      host_impl_.pending_tree()->LayerById(id_));

  // Set some state on the pending layer, make sure it is not clobbered
  // by a sync from the active layer.  This could happen because if the
  // pending layer has not been post-commit initialized it will attempt
  // to sync from the active layer.
  bool default_lcd_text_setting = pending_layer_->is_using_lcd_text();
  pending_layer_->force_set_lcd_text(!default_lcd_text_setting);
  EXPECT_TRUE(pending_layer_->needs_post_commit_initialization());

  host_impl_.ActivatePendingTree();

  active_layer_ = static_cast<FakePictureLayerImpl*>(
      host_impl_.active_tree()->LayerById(id_));

  EXPECT_EQ(0u, active_layer_->num_tilings());
  EXPECT_EQ(!default_lcd_text_setting, active_layer_->is_using_lcd_text());
  EXPECT_FALSE(active_layer_->needs_post_commit_initialization());
}

TEST_F(PictureLayerImplTest, SyncTilingAfterReleaseResource) {
  SetupDefaultTrees(gfx::Size(10, 10));
  host_impl_.active_tree()->UpdateDrawProperties();
  EXPECT_FALSE(host_impl_.active_tree()->needs_update_draw_properties());

  // Contrived unit test of a real crash. A layer is transparent during a
  // context loss, and later becomes opaque, causing active layer SyncTiling to
  // be called.
  const float tile_scale = 2.f;
  active_layer_->ReleaseResources();
  EXPECT_FALSE(active_layer_->tilings()->TilingAtScale(tile_scale));
  pending_layer_->AddTiling(2.f);
  EXPECT_TRUE(active_layer_->tilings()->TilingAtScale(tile_scale));
}

TEST_F(PictureLayerImplTest, TilingWithoutGpuRasterization) {
  gfx::Size default_tile_size(host_impl_.settings().default_tile_size);
  gfx::Size layer_bounds(default_tile_size.width() * 4,
                         default_tile_size.height() * 4);
  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupDefaultTrees(layer_bounds);
  EXPECT_FALSE(pending_layer_->ShouldUseGpuRasterization());
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());
  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  // Should have a low-res and a high-res tiling.
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());
}

TEST_F(PictureLayerImplTest, NoTilingIfDoesNotDrawContent) {
  // Set up layers with tilings.
  SetupDefaultTrees(gfx::Size(10, 10));
  SetContentsScaleOnBothLayers(1.f, 1.f, 1.f, false);
  pending_layer_->PushPropertiesTo(active_layer_);
  EXPECT_TRUE(pending_layer_->DrawsContent());
  EXPECT_TRUE(pending_layer_->CanHaveTilings());
  EXPECT_GE(pending_layer_->num_tilings(), 0u);
  EXPECT_GE(active_layer_->num_tilings(), 0u);

  // Set content to false, which should make CanHaveTilings return false.
  pending_layer_->SetDrawsContent(false);
  EXPECT_FALSE(pending_layer_->DrawsContent());
  EXPECT_FALSE(pending_layer_->CanHaveTilings());

  // No tilings should be pushed to active layer.
  pending_layer_->PushPropertiesTo(active_layer_);
  EXPECT_EQ(0u, active_layer_->num_tilings());
}

TEST_F(PictureLayerImplTest, FirstTilingDuringPinch) {
  SetupDefaultTrees(gfx::Size(10, 10));
  host_impl_.PinchGestureBegin();
  float high_res_scale = 2.3f;
  SetContentsScaleOnBothLayers(high_res_scale, 1.f, 1.f, false);

  ASSERT_GE(pending_layer_->num_tilings(), 0u);
  EXPECT_FLOAT_EQ(high_res_scale,
                  pending_layer_->HighResTiling()->contents_scale());
}

TEST_F(PictureLayerImplTest, FirstTilingTooSmall) {
  SetupDefaultTrees(gfx::Size(10, 10));
  host_impl_.PinchGestureBegin();
  float high_res_scale = 0.0001f;
  EXPECT_GT(pending_layer_->MinimumContentsScale(), high_res_scale);

  SetContentsScaleOnBothLayers(high_res_scale, 1.f, 1.f, false);

  ASSERT_GE(pending_layer_->num_tilings(), 0u);
  EXPECT_FLOAT_EQ(pending_layer_->MinimumContentsScale(),
                  pending_layer_->HighResTiling()->contents_scale());
}

TEST_F(PictureLayerImplTest, PinchingTooSmall) {
  SetupDefaultTrees(gfx::Size(10, 10));

  float contents_scale = 0.15f;
  SetContentsScaleOnBothLayers(contents_scale, 1.f, 1.f, false);

  ASSERT_GE(pending_layer_->num_tilings(), 0u);
  EXPECT_FLOAT_EQ(contents_scale,
                  pending_layer_->HighResTiling()->contents_scale());

  host_impl_.PinchGestureBegin();

  float page_scale = 0.0001f;
  EXPECT_LT(page_scale * contents_scale,
            pending_layer_->MinimumContentsScale());


  SetContentsScaleOnBothLayers(contents_scale, 1.f, page_scale, false);
  ASSERT_GE(pending_layer_->num_tilings(), 0u);
  EXPECT_FLOAT_EQ(pending_layer_->MinimumContentsScale(),
                  pending_layer_->HighResTiling()->contents_scale());
}

class DeferredInitPictureLayerImplTest : public PictureLayerImplTest {
 public:
  virtual void InitializeRenderer() OVERRIDE {
    host_impl_.InitializeRenderer(FakeOutputSurface::CreateDeferredGL(
        scoped_ptr<SoftwareOutputDevice>(new SoftwareOutputDevice))
                                      .PassAs<OutputSurface>());
  }

  virtual void SetUp() OVERRIDE {
    PictureLayerImplTest::SetUp();

    // Create some default active and pending trees.
    gfx::Size tile_size(100, 100);
    gfx::Size layer_bounds(400, 400);

    scoped_refptr<FakePicturePileImpl> pending_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
    scoped_refptr<FakePicturePileImpl> active_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

    SetupTrees(pending_pile, active_pile);
  }
};

// This test is really a LayerTreeHostImpl test, in that it makes sure
// that trees need update draw properties after deferred initialization.
// However, this is also a regression test for PictureLayerImpl in that
// not having this update will cause a crash.
TEST_F(DeferredInitPictureLayerImplTest,
       PreventUpdateTilePrioritiesDuringLostContext) {
  host_impl_.pending_tree()->UpdateDrawProperties();
  host_impl_.active_tree()->UpdateDrawProperties();
  EXPECT_FALSE(host_impl_.pending_tree()->needs_update_draw_properties());
  EXPECT_FALSE(host_impl_.active_tree()->needs_update_draw_properties());

  FakeOutputSurface* fake_output_surface =
      static_cast<FakeOutputSurface*>(host_impl_.output_surface());
  ASSERT_TRUE(fake_output_surface->InitializeAndSetContext3d(
      TestContextProvider::Create(), NULL));

  // These will crash PictureLayerImpl if this is not true.
  ASSERT_TRUE(host_impl_.pending_tree()->needs_update_draw_properties());
  ASSERT_TRUE(host_impl_.active_tree()->needs_update_draw_properties());
  host_impl_.active_tree()->UpdateDrawProperties();
}

class HybridRasterizationPictureLayerImplTest : public PictureLayerImplTest {
 public:
  HybridRasterizationPictureLayerImplTest()
      : PictureLayerImplTest(HybridRasterizationSettings()) {}
};

TEST_F(HybridRasterizationPictureLayerImplTest, Tiling) {
  gfx::Size default_tile_size(host_impl_.settings().default_tile_size);
  gfx::Size layer_bounds(default_tile_size.width() * 4,
                         default_tile_size.height() * 4);
  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupDefaultTrees(layer_bounds);
  EXPECT_FALSE(pending_layer_->ShouldUseGpuRasterization());
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());
  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  // Should have a low-res and a high-res tiling.
  ASSERT_EQ(2u, pending_layer_->tilings()->num_tilings());

  pending_layer_->SetHasGpuRasterizationHint(true);
  EXPECT_TRUE(pending_layer_->ShouldUseGpuRasterization());
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());
  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  // Should only have the high-res tiling.
  ASSERT_EQ(1u, pending_layer_->tilings()->num_tilings());
}

TEST_F(HybridRasterizationPictureLayerImplTest,
       HighResTilingDuringAnimationForCpuRasterization) {
  gfx::Size tile_size(host_impl_.settings().default_tile_size);
  SetupDefaultTrees(tile_size);
  pending_layer_->SetHasGpuRasterizationHint(false);
  active_layer_->SetHasGpuRasterizationHint(false);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  bool animating_transform = false;

  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 1.f);

  // Changing contents scale shouldn't affect tiling resolution during the
  // animation, since we're CPU-rasterizing.
  animating_transform = true;
  contents_scale = 2.f;

  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 1.f);

  // Once we stop animating, a new high-res tiling should be created.
  animating_transform = false;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 2.f);
}

TEST_F(HybridRasterizationPictureLayerImplTest,
       HighResTilingDuringAnimationForGpuRasterization) {
  gfx::Size tile_size(host_impl_.settings().default_tile_size);
  SetupDefaultTrees(tile_size);
  pending_layer_->SetHasGpuRasterizationHint(true);
  active_layer_->SetHasGpuRasterizationHint(true);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  bool animating_transform = false;

  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 1.f);

  // Changing contents scale during an animation should cause tiling resolution
  // to change, since we're GPU-rasterizing.
  animating_transform = true;
  contents_scale = 2.f;

  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 2.f);

  // Since we're re-rasterizing during the animation, scales smaller than 1
  // should be respected.
  contents_scale = 0.5f;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 0.5f);

  // Tiling resolution should also update once we stop animating.
  contents_scale = 4.f;
  animating_transform = false;
  SetContentsScaleOnBothLayers(
      contents_scale, device_scale, page_scale, animating_transform);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale(), 4.f);
}

class GpuRasterizationPictureLayerImplTest : public PictureLayerImplTest {
 public:
  GpuRasterizationPictureLayerImplTest()
      : PictureLayerImplTest(GpuRasterizationSettings()) {}
};

TEST_F(GpuRasterizationPictureLayerImplTest, Tiling) {
  gfx::Size default_tile_size(host_impl_.settings().default_tile_size);
  gfx::Size layer_bounds(default_tile_size.width() * 4,
                         default_tile_size.height() * 4);
  float result_scale_x, result_scale_y;
  gfx::Size result_bounds;

  SetupDefaultTrees(layer_bounds);
  pending_layer_->SetHasGpuRasterizationHint(true);
  EXPECT_TRUE(pending_layer_->ShouldUseGpuRasterization());
  EXPECT_EQ(0u, pending_layer_->tilings()->num_tilings());
  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  // Should only have the high-res tiling.
  ASSERT_EQ(1u, pending_layer_->tilings()->num_tilings());

  pending_layer_->SetHasGpuRasterizationHint(false);
  EXPECT_TRUE(pending_layer_->ShouldUseGpuRasterization());
  // Should still have the high-res tiling.
  EXPECT_EQ(1u, pending_layer_->tilings()->num_tilings());
  pending_layer_->CalculateContentsScale(
      1.f, 1.f, 1.f, false, &result_scale_x, &result_scale_y, &result_bounds);
  // Should still only have the high-res tiling.
  ASSERT_EQ(1u, pending_layer_->tilings()->num_tilings());
}

TEST_F(PictureLayerImplTest, LayerRasterTileIterator) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupPendingTree(pending_pile);

  ASSERT_TRUE(pending_layer_->CanHaveTilings());

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;

  pending_layer_->AddTiling(low_res_factor);
  pending_layer_->AddTiling(0.3f);
  pending_layer_->AddTiling(0.7f);
  PictureLayerTiling* high_res_tiling = pending_layer_->AddTiling(1.0f);
  pending_layer_->AddTiling(2.0f);

  host_impl_.SetViewportSize(gfx::Size(500, 500));
  host_impl_.pending_tree()->UpdateDrawProperties();

  PictureLayerImpl::LayerRasterTileIterator it;
  EXPECT_FALSE(it);

  std::set<Tile*> unique_tiles;
  bool reached_prepaint = false;
  size_t non_ideal_tile_count = 0u;
  size_t low_res_tile_count = 0u;
  size_t high_res_tile_count = 0u;
  for (it = PictureLayerImpl::LayerRasterTileIterator(pending_layer_, false);
       it;
       ++it) {
    Tile* tile = *it;
    TilePriority priority = tile->priority(PENDING_TREE);

    EXPECT_TRUE(tile);

    // Non-high res tiles only get visible tiles. Also, prepaint should only
    // come at the end of the iteration.
    if (priority.resolution != HIGH_RESOLUTION)
      EXPECT_EQ(TilePriority::NOW, priority.priority_bin);
    else if (reached_prepaint)
      EXPECT_NE(TilePriority::NOW, priority.priority_bin);
    else
      reached_prepaint = priority.priority_bin != TilePriority::NOW;

    non_ideal_tile_count += priority.resolution == NON_IDEAL_RESOLUTION;
    low_res_tile_count += priority.resolution == LOW_RESOLUTION;
    high_res_tile_count += priority.resolution == HIGH_RESOLUTION;

    unique_tiles.insert(tile);
  }

  EXPECT_TRUE(reached_prepaint);
  EXPECT_EQ(0u, non_ideal_tile_count);
  EXPECT_EQ(1u, low_res_tile_count);
  EXPECT_EQ(16u, high_res_tile_count);
  EXPECT_EQ(low_res_tile_count + high_res_tile_count + non_ideal_tile_count,
            unique_tiles.size());

  std::vector<Tile*> high_res_tiles = high_res_tiling->AllTilesForTesting();
  for (std::vector<Tile*>::iterator tile_it = high_res_tiles.begin();
       tile_it != high_res_tiles.end();
       ++tile_it) {
    Tile* tile = *tile_it;
    ManagedTileState::TileVersion& tile_version =
        tile->GetTileVersionForTesting(
            tile->DetermineRasterModeForTree(ACTIVE_TREE));
    tile_version.SetSolidColorForTesting(SK_ColorRED);
  }

  non_ideal_tile_count = 0;
  low_res_tile_count = 0;
  high_res_tile_count = 0;
  for (it = PictureLayerImpl::LayerRasterTileIterator(pending_layer_, false);
       it;
       ++it) {
    Tile* tile = *it;
    TilePriority priority = tile->priority(PENDING_TREE);

    EXPECT_TRUE(tile);

    non_ideal_tile_count += priority.resolution == NON_IDEAL_RESOLUTION;
    low_res_tile_count += priority.resolution == LOW_RESOLUTION;
    high_res_tile_count += priority.resolution == HIGH_RESOLUTION;
  }

  EXPECT_EQ(0u, non_ideal_tile_count);
  EXPECT_EQ(1u, low_res_tile_count);
  EXPECT_EQ(0u, high_res_tile_count);
}

TEST_F(PictureLayerImplTest, LayerEvictionTileIterator) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SetupPendingTree(pending_pile);

  ASSERT_TRUE(pending_layer_->CanHaveTilings());

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;

  std::vector<PictureLayerTiling*> tilings;
  tilings.push_back(pending_layer_->AddTiling(low_res_factor));
  tilings.push_back(pending_layer_->AddTiling(0.3f));
  tilings.push_back(pending_layer_->AddTiling(0.7f));
  tilings.push_back(pending_layer_->AddTiling(1.0f));
  tilings.push_back(pending_layer_->AddTiling(2.0f));

  host_impl_.SetViewportSize(gfx::Size(500, 500));
  host_impl_.pending_tree()->UpdateDrawProperties();

  std::vector<Tile*> all_tiles;
  for (std::vector<PictureLayerTiling*>::iterator tiling_iterator =
           tilings.begin();
       tiling_iterator != tilings.end();
       ++tiling_iterator) {
    std::vector<Tile*> tiles = (*tiling_iterator)->AllTilesForTesting();
    std::copy(tiles.begin(), tiles.end(), std::back_inserter(all_tiles));
  }

  std::set<Tile*> all_tiles_set(all_tiles.begin(), all_tiles.end());

  bool mark_required = false;
  for (std::vector<Tile*>::iterator it = all_tiles.begin();
       it != all_tiles.end();
       ++it) {
    Tile* tile = *it;
    if (mark_required)
      tile->MarkRequiredForActivation();
    mark_required = !mark_required;
  }

  // Sanity checks.
  EXPECT_EQ(91u, all_tiles.size());
  EXPECT_EQ(91u, all_tiles_set.size());

  // Empty iterator.
  PictureLayerImpl::LayerEvictionTileIterator it;
  EXPECT_FALSE(it);

  // Tiles don't have resources yet.
  it = PictureLayerImpl::LayerEvictionTileIterator(pending_layer_);
  EXPECT_FALSE(it);

  host_impl_.tile_manager()->InitializeTilesWithResourcesForTesting(all_tiles);

  std::set<Tile*> unique_tiles;
  float expected_scales[] = {2.0f, 0.3f, 0.7f, low_res_factor, 1.0f};
  size_t scale_index = 0;
  bool reached_visible = false;
  bool reached_required = false;
  Tile* last_tile = NULL;
  for (it = PictureLayerImpl::LayerEvictionTileIterator(pending_layer_); it;
       ++it) {
    Tile* tile = *it;
    if (!last_tile)
      last_tile = tile;

    EXPECT_TRUE(tile);

    TilePriority priority = tile->priority(PENDING_TREE);

    if (priority.priority_bin == TilePriority::NOW) {
      reached_visible = true;
      last_tile = tile;
      break;
    }

    if (reached_required) {
      EXPECT_TRUE(tile->required_for_activation());
    } else if (tile->required_for_activation()) {
      reached_required = true;
      scale_index = 0;
    }

    while (std::abs(tile->contents_scale() - expected_scales[scale_index]) >
           std::numeric_limits<float>::epsilon()) {
      ++scale_index;
      ASSERT_LT(scale_index, arraysize(expected_scales));
    }

    EXPECT_FLOAT_EQ(tile->contents_scale(), expected_scales[scale_index]);
    unique_tiles.insert(tile);

    // If the tile is the same rough bin as last tile (same activation, bin, and
    // scale), then distance should be decreasing.
    if (tile->required_for_activation() ==
            last_tile->required_for_activation() &&
        priority.priority_bin ==
            last_tile->priority(PENDING_TREE).priority_bin &&
        std::abs(tile->contents_scale() - last_tile->contents_scale()) <
            std::numeric_limits<float>::epsilon()) {
      EXPECT_LE(priority.distance_to_visible,
                last_tile->priority(PENDING_TREE).distance_to_visible);
    }

    last_tile = tile;
  }

  EXPECT_TRUE(reached_visible);
  EXPECT_TRUE(reached_required);
  EXPECT_EQ(65u, unique_tiles.size());

  scale_index = 0;
  reached_required = false;
  for (; it; ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);

    TilePriority priority = tile->priority(PENDING_TREE);
    EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

    if (reached_required) {
      EXPECT_TRUE(tile->required_for_activation());
    } else if (tile->required_for_activation()) {
      reached_required = true;
      scale_index = 0;
    }

    while (std::abs(tile->contents_scale() - expected_scales[scale_index]) >
           std::numeric_limits<float>::epsilon()) {
      ++scale_index;
      ASSERT_LT(scale_index, arraysize(expected_scales));
    }

    EXPECT_FLOAT_EQ(tile->contents_scale(), expected_scales[scale_index]);
    unique_tiles.insert(tile);
  }

  EXPECT_TRUE(reached_required);
  EXPECT_EQ(all_tiles_set.size(), unique_tiles.size());
}

}  // namespace
}  // namespace cc
