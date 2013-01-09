// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_RENDER_PASS_TEST_UTILS_H_
#define CC_TEST_RENDER_PASS_TEST_UTILS_H_

#include "cc/render_pass.h"
#include "cc/scoped_ptr_vector.h"
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
TestRenderPass* addRenderPass(
    RenderPassList& passList,
    RenderPass::Id id,
    const gfx::Rect& outputRect,
    const gfx::Transform& rootTransform);

// Adds a solid quad to a given render pass.
SolidColorDrawQuad* addQuad(TestRenderPass* pass,
                            const gfx::Rect& rect,
                            SkColor color);

// Adds a render pass quad to an existing render pass.
void addRenderPassQuad(TestRenderPass* toPass,
                       TestRenderPass* contributingPass);

}  // namespace cc

#endif  // CC_TEST_RENDER_PASS_TEST_UTILS_H_
