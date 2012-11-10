// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/renderer.h"

namespace cc {

bool Renderer::haveCachedResourcesForRenderPassId(RenderPass::Id) const
{
    return false;
}

bool Renderer::isContextLost()
{
    return false;
}

}  // namespace cc
