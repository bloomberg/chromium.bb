// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RENDERING_STATS_H_
#define CC_RENDERING_STATS_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"

namespace cc {

struct CC_EXPORT RenderingStats {
    // FIXME: Rename these to animationFrameCount and screenFrameCount, crbug.com/138641.
    int64 numAnimationFrames;
    int64 numFramesSentToScreen;
    int64 droppedFrameCount;
    double totalPaintTimeInSeconds;
    double totalRasterizeTimeInSeconds;
    double totalCommitTimeInSeconds;
    int64 totalCommitCount;
    int64 totalPixelsPainted;
    int64 totalPixelsRasterized;
    int64 numImplThreadScrolls;
    int64 numMainThreadScrolls;
    int64 numLayersDrawn;

    RenderingStats();
};

}  // namespace cc

#endif  // CC_RENDERING_STATS_H_
