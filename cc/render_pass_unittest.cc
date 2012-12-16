// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_pass.h"

#include <public/WebFilterOperations.h>

#include "cc/checkerboard_draw_quad.h"
#include "cc/math_util.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/render_pass_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/transform.h"

using WebKit::WebFilterOperation;
using WebKit::WebFilterOperations;
using cc::TestRenderPass;

namespace cc {
namespace {

struct RenderPassSize {
    // If you add a new field to this class, make sure to add it to the copy() tests.
    RenderPass::Id m_id;
    QuadList m_quadList;
    SharedQuadStateList m_sharedQuadStateList;
    gfx::Transform m_transformToRootTarget;
    gfx::Rect m_outputRect;
    gfx::RectF m_damageRect;
    bool m_hasTransparentBackground;
    bool m_hasOcclusionFromOutsideTargetSurface;
    WebKit::WebFilterOperations m_filters;
    WebKit::WebFilterOperations m_backgroundFilters;
    SkImageFilter* m_filter;
};

TEST(RenderPassTest, copyShouldBeIdenticalExceptIdAndQuads)
{
    RenderPass::Id id(3, 2);
    gfx::Rect outputRect(45, 22, 120, 13);
    gfx::Transform transformToRoot = MathUtil::createGfxTransform(1, 0.5, 0.5, -0.5, -1, 0);
    gfx::Rect damageRect(56, 123, 19, 43);
    bool hasTransparentBackground = true;
    bool hasOcclusionFromOutsideTargetSurface = true;
    WebFilterOperations filters;
    WebFilterOperations backgroundFilters;

    filters.append(WebFilterOperation::createGrayscaleFilter(0.2f));
    backgroundFilters.append(WebFilterOperation::createInvertFilter(0.2f));
    skia::RefPtr<SkImageFilter> filter = skia::AdoptRef(new SkBlurImageFilter(SK_Scalar1, SK_Scalar1));

    scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
    pass->SetAll(id,
                 outputRect,
                 damageRect,
                 transformToRoot,
                 hasTransparentBackground,
                 hasOcclusionFromOutsideTargetSurface,
                 filters,
                 filter,
                 backgroundFilters);

    // Stick a quad in the pass, this should not get copied.
    scoped_ptr<SharedQuadState> sharedState = SharedQuadState::Create();
    sharedState->SetAll(gfx::Transform(), gfx::Rect(), gfx::Rect(), gfx::Rect(), false, 1);
    pass->AppendSharedQuadState(sharedState.Pass());

    scoped_ptr<CheckerboardDrawQuad> checkerboardQuad = CheckerboardDrawQuad::Create();
    checkerboardQuad->SetNew(pass->shared_quad_state_list.last(), gfx::Rect(), SkColor());
    pass->quad_list.append(checkerboardQuad.PassAs<DrawQuad>());

    RenderPass::Id newId(63, 4);

    scoped_ptr<RenderPass> copy = pass->Copy(newId);
    EXPECT_EQ(newId, copy->id);
    EXPECT_RECT_EQ(pass->output_rect, copy->output_rect);
    EXPECT_EQ(pass->transform_to_root_target, copy->transform_to_root_target);
    EXPECT_RECT_EQ(pass->damage_rect, copy->damage_rect);
    EXPECT_EQ(pass->has_transparent_background, copy->has_transparent_background);
    EXPECT_EQ(pass->has_occlusion_from_outside_target_surface, copy->has_occlusion_from_outside_target_surface);
    EXPECT_EQ(pass->filters, copy->filters);
    EXPECT_EQ(pass->filter, copy->filter);
    EXPECT_EQ(pass->background_filters, copy->background_filters);
    EXPECT_EQ(0u, copy->quad_list.size());

    EXPECT_EQ(sizeof(RenderPassSize), sizeof(RenderPass));
}

}  // namespace
}  // namespace cc
