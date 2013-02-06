// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_APPEND_QUADS_DATA_H_
#define CC_APPEND_QUADS_DATA_H_

#include "base/basictypes.h"
#include "cc/render_pass.h"

namespace cc {

struct AppendQuadsData {
    AppendQuadsData()
        : hadOcclusionFromOutsideTargetSurface(false)
        , hadIncompleteTile(false)
        , numMissingTiles(0)
        , renderPassId(0, 0)
    {
    }

    explicit AppendQuadsData(RenderPass::Id renderPassId)
        : hadOcclusionFromOutsideTargetSurface(false)
        , hadIncompleteTile(false)
        , numMissingTiles(0)
        , renderPassId(renderPassId)
    {
    }

    // Set by the QuadCuller.
    bool hadOcclusionFromOutsideTargetSurface;
    // Set by the layer appending quads.
    bool hadIncompleteTile;
    // Set by the layer appending quads.
    int64 numMissingTiles;
    // Given to the layer appending quads.
    const RenderPass::Id renderPassId;
};

}
#endif  // CC_APPEND_QUADS_DATA_H_
