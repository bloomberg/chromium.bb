// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCRenderer.h"

namespace cc {

bool CCRenderer::haveCachedResourcesForRenderPassId(CCRenderPass::Id) const
{
    return false;
}

bool CCRenderer::isContextLost()
{
    return false;
}

}
