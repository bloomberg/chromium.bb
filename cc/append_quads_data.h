// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCAppendQuadsData_h
#define CCAppendQuadsData_h

#include "cc/render_pass.h"

namespace cc {

struct AppendQuadsData {
    AppendQuadsData()
        : hadOcclusionFromOutsideTargetSurface(false)
        , hadMissingTiles(false)
        , renderPassId(0, 0)
    {
    }

    explicit AppendQuadsData(RenderPass::Id renderPassId)
        : hadOcclusionFromOutsideTargetSurface(false)
        , hadMissingTiles(false)
        , renderPassId(renderPassId)
    {
    }

    // Set by the QuadCuller.
    bool hadOcclusionFromOutsideTargetSurface;
    // Set by the layer appending quads.
    bool hadMissingTiles;
    // Given to the layer appending quads.
    const RenderPass::Id renderPassId;
};

}
#endif // CCCCAppendQuadsData_h
