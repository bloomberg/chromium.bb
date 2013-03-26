// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_RENDER_PASS_TEST_UTILS_H_
#define CC_TEST_RENDER_PASS_TEST_UTILS_H_

#include "cc/base/scoped_ptr_vector.h"
#include "cc/quads/render_pass.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Rect;
class Transform;
}

namespace cc {

class SolidColorDrawQuad;
class TestRenderPass;

// Adds a new render pass with the provided properties to the given
// render pass list.
TestRenderPass* AddRenderPass(
    RenderPassList* pass_list,
    RenderPass::Id id,
    gfx::Rect output_rect,
    const gfx::Transform& root_transform);

// Adds a solid quad to a given render pass.
SolidColorDrawQuad* AddQuad(TestRenderPass* pass,
                            gfx::Rect rect,
                            SkColor color);

// Adds a solid quad to a given render pass and sets is_clipped=true.
SolidColorDrawQuad* AddClippedQuad(TestRenderPass* pass,
                                   gfx::Rect rect,
                                   SkColor color);

// Adds a render pass quad to an existing render pass.
void AddRenderPassQuad(TestRenderPass* to_pass,
                       TestRenderPass* contributing_pass);

}  // namespace cc

#endif  // CC_TEST_RENDER_PASS_TEST_UTILS_H_
