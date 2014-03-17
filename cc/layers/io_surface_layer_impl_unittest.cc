// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/io_surface_layer_impl.h"

#include "cc/layers/append_quads_data.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/trees/layer_tree_host_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(IOSurfaceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;
  MockQuadCuller quad_culler;
  AppendQuadsData data;

  IOSurfaceLayerImpl* io_surface_layer_impl =
      impl.AddChildToRoot<IOSurfaceLayerImpl>();
  io_surface_layer_impl->SetAnchorPoint(gfx::PointF());
  io_surface_layer_impl->SetBounds(layer_size);
  io_surface_layer_impl->SetContentBounds(layer_size);
  io_surface_layer_impl->SetDrawsContent(true);

  impl.CalcDrawProps(viewport_size);

  // No occlusion.
  {
    gfx::Rect occluded;
    quad_culler.clear_lists();
    quad_culler.set_occluded_content_rect(occluded);
    io_surface_layer_impl->AppendQuads(&quad_culler, &data);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(quad_culler.quad_list(),
                                                 gfx::Rect(viewport_size));
    EXPECT_EQ(1u, quad_culler.quad_list().size());
  }

  // Full occlusion.
  {
    gfx::Rect occluded = io_surface_layer_impl->visible_content_rect();
    quad_culler.clear_lists();
    quad_culler.set_occluded_content_rect(occluded);
    io_surface_layer_impl->AppendQuads(&quad_culler, &data);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(quad_culler.quad_list(),
                                                 gfx::Rect());
    EXPECT_EQ(quad_culler.quad_list().size(), 0u);
  }

  // Partial occlusion.
  {
    gfx::Rect occluded(200, 0, 800, 1000);
    quad_culler.clear_lists();
    quad_culler.set_occluded_content_rect(occluded);
    io_surface_layer_impl->AppendQuads(&quad_culler, &data);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
        quad_culler.quad_list(),
        gfx::Rect(viewport_size),
        occluded,
        &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, quad_culler.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

}  // namespace
}  // namespace cc
