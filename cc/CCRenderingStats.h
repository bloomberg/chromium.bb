// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderingStats_h
#define CCRenderingStats_h

namespace cc {

struct CCRenderingStats {
    // FIXME: Rename these to animationFrameCount and screenFrameCount, crbug.com/138641.
    int numAnimationFrames;
    int numFramesSentToScreen;
    int droppedFrameCount;
    double totalPaintTimeInSeconds;
    double totalRasterizeTimeInSeconds;

    CCRenderingStats()
        : numAnimationFrames(0)
        , numFramesSentToScreen(0)
        , droppedFrameCount(0)
        , totalPaintTimeInSeconds(0)
        , totalRasterizeTimeInSeconds(0)
    {
    }
};

}

#endif
