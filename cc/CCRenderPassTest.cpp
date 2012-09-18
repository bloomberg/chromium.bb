// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCRenderPass.h"

#include "CCCheckerboardDrawQuad.h"
#include "CCGeometryTestUtils.h"
#include <gtest/gtest.h>
#include <public/WebFilterOperations.h>
#include <public/WebTransformationMatrix.h>

using WebKit::WebFilterOperation;
using WebKit::WebFilterOperations;
using WebKit::WebTransformationMatrix;

using namespace cc;

namespace {

class CCTestRenderPass : public CCRenderPass {
public:
    CCQuadList& quadList() { return m_quadList; }
    CCSharedQuadStateList& sharedQuadStateList() { return m_sharedQuadStateList; }
};

struct CCRenderPassSize {
    // If you add a new field to this class, make sure to add it to the copy() tests.
    CCRenderPass::Id m_id;
    CCQuadList m_quadList;
    CCSharedQuadStateList m_sharedQuadStateList;
    WebKit::WebTransformationMatrix m_transformToRootTarget;
    IntRect m_outputRect;
    FloatRect m_damageRect;
    bool m_hasTransparentBackground;
    bool m_hasOcclusionFromOutsideTargetSurface;
    WebKit::WebFilterOperations m_filters;
    WebKit::WebFilterOperations m_backgroundFilters;
};

TEST(CCRenderPassTest, copyShouldBeIdenticalExceptIdAndQuads)
{
    CCRenderPass::Id id(3, 2);
    IntRect outputRect(45, 22, 120, 13);
    WebTransformationMatrix transformToRoot(1, 0.5, 0.5, -0.5, -1, 0);

    OwnPtr<CCRenderPass> pass(CCRenderPass::create(id, outputRect, transformToRoot));

    IntRect damageRect(56, 123, 19, 43);
    bool hasTransparentBackground = true;
    bool hasOcclusionFromOutsideTargetSurface = true;
    WebFilterOperations filters;
    WebFilterOperations backgroundFilters;

    filters.append(WebFilterOperation::createGrayscaleFilter(0.2f));
    backgroundFilters.append(WebFilterOperation::createInvertFilter(0.2f));

    pass->setDamageRect(damageRect);
    pass->setHasTransparentBackground(hasTransparentBackground);
    pass->setHasOcclusionFromOutsideTargetSurface(hasOcclusionFromOutsideTargetSurface);
    pass->setFilters(filters);
    pass->setBackgroundFilters(backgroundFilters);

    // Stick a quad in the pass, this should not get copied.
    CCTestRenderPass* testPass = static_cast<CCTestRenderPass*>(pass.get());
    testPass->sharedQuadStateList().append(CCSharedQuadState::create(WebTransformationMatrix(), IntRect(), IntRect(), 1, false));
    testPass->quadList().append(CCCheckerboardDrawQuad::create(testPass->sharedQuadStateList().last().get(), IntRect()));

    CCRenderPass::Id newId(63, 4);

    OwnPtr<CCRenderPass> copy(pass->copy(newId));
    EXPECT_EQ(newId, copy->id());
    EXPECT_RECT_EQ(pass->outputRect(), copy->outputRect());
    EXPECT_EQ(pass->transformToRootTarget(), copy->transformToRootTarget());
    EXPECT_RECT_EQ(pass->damageRect(), copy->damageRect());
    EXPECT_EQ(pass->hasTransparentBackground(), copy->hasTransparentBackground());
    EXPECT_EQ(pass->hasOcclusionFromOutsideTargetSurface(), copy->hasOcclusionFromOutsideTargetSurface());
    EXPECT_EQ(pass->filters(), copy->filters());
    EXPECT_EQ(pass->backgroundFilters(), copy->backgroundFilters());
    EXPECT_EQ(0u, copy->quadList().size());

    EXPECT_EQ(sizeof(CCRenderPassSize), sizeof(CCRenderPass));
}

} // namespace
