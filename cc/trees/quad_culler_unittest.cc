// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/quad_culler.h"

#include "cc/base/math_util.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/tiled_layer_impl.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/layer_tiling_data.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestOcclusionTrackerImpl : public OcclusionTrackerImpl {
 public:
  TestOcclusionTrackerImpl(gfx::Rect scissor_rect_in_screen,
                           bool record_metrics_for_frame = true)
      : OcclusionTrackerImpl(scissor_rect_in_screen, record_metrics_for_frame),
        scissor_rect_in_screen_(scissor_rect_in_screen) {}

 protected:
  virtual gfx::Rect LayerScissorRectInTargetSurface(
      const LayerImpl* layer) const {
    return scissor_rect_in_screen_;
  }

 private:
  gfx::Rect scissor_rect_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(TestOcclusionTrackerImpl);
};

typedef LayerIterator<LayerImpl,
                      std::vector<LayerImpl*>,
                      RenderSurfaceImpl,
                      LayerIteratorActions::FrontToBack> LayerIteratorType;

class QuadCullerTest : public testing::Test {
 public:
  QuadCullerTest()
      : host_impl_(&proxy_),
        layer_id_(1) {}

  scoped_ptr<TiledLayerImpl> MakeLayer(
      TiledLayerImpl* parent,
      const gfx::Transform& draw_transform,
      gfx::Rect layer_rect,
      float opacity,
      bool opaque,
      gfx::Rect layer_opaque_rect,
      std::vector<LayerImpl*>& surface_layer_list) {
    scoped_ptr<TiledLayerImpl> layer =
        TiledLayerImpl::Create(host_impl_.active_tree(), layer_id_++);
    scoped_ptr<LayerTilingData> tiler = LayerTilingData::Create(
        gfx::Size(100, 100), LayerTilingData::NO_BORDER_TEXELS);
    tiler->SetBounds(layer_rect.size());
    layer->SetTilingData(*tiler);
    layer->set_skips_draw(false);
    layer->draw_properties().target_space_transform = draw_transform;
    layer->draw_properties().screen_space_transform = draw_transform;
    layer->draw_properties().visible_content_rect = layer_rect;
    layer->draw_properties().opacity = opacity;
    layer->SetContentsOpaque(opaque);
    layer->SetBounds(layer_rect.size());
    layer->SetContentBounds(layer_rect.size());

    ResourceProvider::ResourceId resource_id = 1;
    for (int i = 0; i < tiler->num_tiles_x(); ++i) {
      for (int j = 0; j < tiler->num_tiles_y(); ++j) {
        gfx::Rect tile_opaque_rect =
            opaque
            ? tiler->tile_bounds(i, j)
            : gfx::IntersectRects(tiler->tile_bounds(i, j), layer_opaque_rect);
        layer->PushTileProperties(i, j, resource_id++, tile_opaque_rect, false);
      }
    }

    gfx::Rect rect_in_target = MathUtil::MapClippedRect(
        layer->draw_transform(), layer->visible_content_rect());
    if (!parent) {
      layer->CreateRenderSurface();
      layer->render_surface()->SetContentRect(rect_in_target);
      surface_layer_list.push_back(layer.get());
      layer->render_surface()->layer_list().push_back(layer.get());
    } else {
      layer->draw_properties().render_target = parent->render_target();
      parent->render_surface()->layer_list().push_back(layer.get());
      rect_in_target.Union(MathUtil::MapClippedRect(
          parent->draw_transform(), parent->visible_content_rect()));
      parent->render_surface()->SetContentRect(rect_in_target);
    }
    layer->draw_properties().drawable_content_rect = rect_in_target;

    return layer.Pass();
  }

  void AppendQuads(QuadList& quad_list,
                   SharedQuadStateList& shared_state_list,
                   TiledLayerImpl* layer,
                   LayerIteratorType& it,
                   OcclusionTrackerImpl& occlusion_tracker) {
    occlusion_tracker.EnterLayer(it);
    QuadCuller quad_culler(
        &quad_list, &shared_state_list, layer, occlusion_tracker, false, false);
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    occlusion_tracker.LeaveLayer(it);
    ++it;
  }

 protected:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  int layer_id_;

  DISALLOW_COPY_AND_ASSIGN(QuadCullerTest);
};

#define DECLARE_AND_INITIALIZE_TEST_QUADS()                                    \
  QuadList quad_list;                                                          \
  SharedQuadStateList shared_state_list;                                       \
  std::vector<LayerImpl*> render_surface_layer_list;                           \
  gfx::Transform child_transform;                                              \
  gfx::Size root_size = gfx::Size(300, 300);                                   \
  gfx::Rect root_rect = gfx::Rect(root_size);                                  \
  gfx::Size child_size = gfx::Size(200, 200);                                  \
  gfx::Rect child_rect = gfx::Rect(child_size);

TEST_F(QuadCullerTest, VerifyNoCulling) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1,
                                                     false,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 13u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              40000,
              1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, VerifyCullChildLinesUpTopLeft) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 9u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              40000,
              1);
}

TEST_F(QuadCullerTest, VerifyCullWhenChildOpacityNotOne) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     0.9f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 13u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              40000,
              1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, VerifyCullWhenChildOpaqueFlagFalse) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     false,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 13u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              40000,
              1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, VerifyCullCenterTileOnly) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(50, 50);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  ASSERT_EQ(quad_list.size(), 12u);

  gfx::Rect quad_visible_rect1 = quad_list[5]->visible_rect;
  EXPECT_EQ(quad_visible_rect1.height(), 50);

  gfx::Rect quad_visible_rect3 = quad_list[7]->visible_rect;
  EXPECT_EQ(quad_visible_rect3.width(), 50);

  // Next index is 8, not 9, since centre quad culled.
  gfx::Rect quad_visible_rect4 = quad_list[8]->visible_rect;
  EXPECT_EQ(quad_visible_rect4.width(), 50);
  EXPECT_EQ(quad_visible_rect4.x(), 250);

  gfx::Rect quad_visible_rect6 = quad_list[10]->visible_rect;
  EXPECT_EQ(quad_visible_rect6.height(), 50);
  EXPECT_EQ(quad_visible_rect6.y(), 250);

  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 100000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              30000,
              1);
}

TEST_F(QuadCullerTest, VerifyCullCenterTileNonIntegralSize1) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(100, 100);

  // Make the root layer's quad have extent (99.1, 99.1) -> (200.9, 200.9) to
  // make sure it doesn't get culled due to transform rounding.
  gfx::Transform root_transform;
  root_transform.Translate(99.1, 99.1);
  root_transform.Scale(1.018, 1.018);

  root_rect = child_rect = gfx::Rect(0, 0, 100, 100);

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    root_transform,
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 2u);

  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 20363, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, VerifyCullCenterTileNonIntegralSize2) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  // Make the child's quad slightly smaller than, and centred over, the root
  // layer tile.  Verify the child does not cause the quad below to be culled
  // due to rounding.
  child_transform.Translate(100.1, 100.1);
  child_transform.Scale(0.982, 0.982);

  gfx::Transform root_transform;
  root_transform.Translate(100, 100);

  root_rect = child_rect = gfx::Rect(0, 0, 100, 100);

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    root_transform,
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 2u);

  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 19643, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, VerifyCullChildLinesUpBottomRight) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(100, 100);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 9u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              40000,
              1);
}

TEST_F(QuadCullerTest, VerifyCullSubRegion) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(50, 50);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  gfx::Rect child_opaque_rect(child_rect.x() + child_rect.width() / 4,
                              child_rect.y() + child_rect.height() / 4,
                              child_rect.width() / 2,
                              child_rect.height() / 2);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     false,
                                                     child_opaque_rect,
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 12u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              30000,
              1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              10000,
              1);
}

TEST_F(QuadCullerTest, VerifyCullSubRegion2) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(50, 10);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  gfx::Rect child_opaque_rect(child_rect.x() + child_rect.width() / 4,
                              child_rect.y() + child_rect.height() / 4,
                              child_rect.width() / 2,
                              child_rect.height() * 3 / 4);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     false,
                                                     child_opaque_rect,
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 12u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              25000,
              1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              15000,
              1);
}

TEST_F(QuadCullerTest, VerifyCullSubRegionCheckOvercull) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(50, 49);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  gfx::Rect child_opaque_rect(child_rect.x() + child_rect.width() / 4,
                              child_rect.y() + child_rect.height() / 4,
                              child_rect.width() / 2,
                              child_rect.height() / 2);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     false,
                                                     child_opaque_rect,
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 13u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              30000,
              1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              10000,
              1);
}

TEST_F(QuadCullerTest, VerifyNonAxisAlignedQuadsDontOcclude) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  // Use a small rotation so as to not disturb the geometry significantly.
  child_transform.Rotate(1);

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 13u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 130000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

// This test requires some explanation: here we are rotating the quads to be
// culled.  The 2x2 tile child layer remains in the top-left corner, unrotated,
// but the 3x3 tile parent layer is rotated by 1 degree. Of the four tiles the
// child would normally occlude, three will move (slightly) out from under the
// child layer, and one moves further under the child. Only this last tile
// should be culled.
TEST_F(QuadCullerTest, VerifyNonAxisAlignedQuadsSafelyCulled) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  // Use a small rotation so as to not disturb the geometry significantly.
  gfx::Transform parent_transform;
  parent_transform.Rotate(1);

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    parent_transform,
                                                    root_rect,
                                                    1,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 12u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 100600, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              29400,
              1);
}

TEST_F(QuadCullerTest, VerifyCullOutsideScissorOverTile) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();
  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(200, 100, 100, 100));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 1u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 10000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              120000,
              1);
}

TEST_F(QuadCullerTest, VerifyCullOutsideScissorOverCulledTile) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();
  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(100, 100, 100, 100));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 1u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 10000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              120000,
              1);
}

TEST_F(QuadCullerTest, VerifyCullOutsideScissorOverPartialTiles) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();
  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(50, 50, 200, 200));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 9u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 40000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              90000,
              1);
}

TEST_F(QuadCullerTest, VerifyCullOutsideScissorOverNoTiles) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();
  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(500, 500, 100, 100));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 0u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              130000,
              1);
}

TEST_F(QuadCullerTest, VerifyWithoutMetrics) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();
  scoped_ptr<TiledLayerImpl> root_layer =
      MakeLayer(NULL,
                gfx::Transform(),
                root_rect,
                1,
                true,
                gfx::Rect(),
                render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(50, 50, 200, 200),
                                             false);
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(
      quad_list, shared_state_list, child_layer.get(), it, occlusion_tracker);
  AppendQuads(
      quad_list, shared_state_list, root_layer.get(), it, occlusion_tracker);
  EXPECT_EQ(quad_list.size(), 9u);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

}  // namespace
}  // namespace cc
