// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCAppendQuadsData_h
#define CCAppendQuadsData_h

namespace cc {

struct CCAppendQuadsData {
    CCAppendQuadsData()
        : hadOcclusionFromOutsideTargetSurface(false)
        , hadMissingTiles(false)
    {
    }

    // Set by the QuadCuller.
    bool hadOcclusionFromOutsideTargetSurface;
    // Set by the layer appending quads.
    bool hadMissingTiles;
};

}
#endif // CCCCAppendQuadsData_h
