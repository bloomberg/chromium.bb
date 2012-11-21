// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_pass.h"

#include "cc/checkerboard_draw_quad.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/render_pass_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include <public/WebFilterOperations.h>
#include <public/WebTransformationMatrix.h>

using WebKit::WebFilterOperation;
using WebKit::WebFilterOperations;
using WebKit::WebTransformationMatrix;
using WebKitTests::TestRenderPass;

namespace cc {
namespace {

struct RenderPassSize {
    // If you add a new field to this class, make sure to add it to the copy() tests.
    RenderPass::Id m_id;
    QuadList m_quadList;
    SharedQuadStateList m_sharedQuadStateList;
    WebKit::WebTransformationMatrix m_transformToRootTarget;
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
    WebTransformationMatrix transformToRoot(1, 0.5, 0.5, -0.5, -1, 0);

    scoped_ptr<TestRenderPass> pass = TestRenderPass::create(id, outputRect, transformToRoot);

    gfx::Rect damageRect(56, 123, 19, 43);
    bool hasTransparentBackground = true;
    bool hasOcclusionFromOutsideTargetSurface = true;
    WebFilterOperations filters;
    WebFilterOperations backgroundFilters;

    filters.append(WebFilterOperation::createGrayscaleFilter(0.2f));
    backgroundFilters.append(WebFilterOperation::createInvertFilter(0.2f));
    SkAutoTUnref<SkBlurImageFilter> filter(new SkBlurImageFilter(SK_Scalar1, SK_Scalar1));

    pass->setDamageRect(damageRect);
    pass->setHasTransparentBackground(hasTransparentBackground);
    pass->setHasOcclusionFromOutsideTargetSurface(hasOcclusionFromOutsideTargetSurface);
    pass->setFilters(filters);
    pass->setBackgroundFilters(backgroundFilters);
    pass->setFilter(filter);

    // Stick a quad in the pass, this should not get copied.
    scoped_ptr<SharedQuadState> sharedState = SharedQuadState::Create();
    sharedState->SetAll(WebTransformationMatrix(), gfx::Rect(), gfx::Rect(), 1);
    pass->sharedQuadStateList().append(sharedState.Pass());

    scoped_ptr<CheckerboardDrawQuad> checkerboardQuad = CheckerboardDrawQuad::Create();
    checkerboardQuad->SetNew(pass->sharedQuadStateList().last(), gfx::Rect(), SkColor());
    pass->quadList().append(checkerboardQuad.PassAs<DrawQuad>());

    RenderPass::Id newId(63, 4);

    scoped_ptr<RenderPass> copy = pass->copy(newId);
    EXPECT_EQ(newId, copy->id());
    EXPECT_RECT_EQ(pass->outputRect(), copy->outputRect());
    EXPECT_EQ(pass->transformToRootTarget(), copy->transformToRootTarget());
    EXPECT_RECT_EQ(pass->damageRect(), copy->damageRect());
    EXPECT_EQ(pass->hasTransparentBackground(), copy->hasTransparentBackground());
    EXPECT_EQ(pass->hasOcclusionFromOutsideTargetSurface(), copy->hasOcclusionFromOutsideTargetSurface());
    EXPECT_EQ(pass->filters(), copy->filters());
    EXPECT_EQ(pass->backgroundFilters(), copy->backgroundFilters());
    EXPECT_EQ(pass->filter(), copy->filter());
    EXPECT_EQ(0u, copy->quadList().size());

    EXPECT_EQ(sizeof(RenderPassSize), sizeof(RenderPass));
}

}  // namespace
}  // namespace cc
