// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCAppendQuadsData_h
#define CCAppendQuadsData_h

#include "CCRenderPass.h"

namespace cc {

struct CCAppendQuadsData {
    CCAppendQuadsData()
        : hadOcclusionFromOutsideTargetSurface(false)
        , hadMissingTiles(false)
        , renderPassId(0, 0)
    {
    }

    explicit CCAppendQuadsData(CCRenderPass::Id renderPassId)
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
    const CCRenderPass::Id renderPassId;
};

}
#endif // CCCCAppendQuadsData_h
