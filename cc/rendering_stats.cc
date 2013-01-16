// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/rendering_stats.h"

namespace cc {

RenderingStats::RenderingStats()
    : numAnimationFrames(0),
      numFramesSentToScreen(0),
      droppedFrameCount(0),
      totalCommitCount(0),
      totalPixelsPainted(0),
      totalPixelsRasterized(0),
      numImplThreadScrolls(0),
      numMainThreadScrolls(0),
      numLayersDrawn(0),
      numMissingTiles(0),
      totalDeferredImageDecodeCount(0),
      totalDeferredImageCacheHitCount(0),
      totalImageGatheringCount(0) {
}

void RenderingStats::EnumerateFields(Enumerator* enumerator) const {
    enumerator->AddInt64("numAnimationFrames", numAnimationFrames);
    enumerator->AddInt64("numFramesSentToScreen", numFramesSentToScreen);
    enumerator->AddInt64("droppedFrameCount", droppedFrameCount);
    enumerator->AddDouble("totalPaintTimeInSeconds",
                          totalPaintTime.InSecondsF());
    enumerator->AddDouble("totalRasterizeTimeInSeconds",
                          totalRasterizeTime.InSecondsF());
    enumerator->AddDouble("totalCommitTimeInSeconds",
                          totalCommitTime.InSecondsF());
    enumerator->AddInt64("totalCommitCount", totalCommitCount);
    enumerator->AddInt64("totalPixelsPainted", totalPixelsPainted);
    enumerator->AddInt64("totalPixelsRasterized", totalPixelsRasterized);
    enumerator->AddInt64("numImplThreadScrolls", numImplThreadScrolls);
    enumerator->AddInt64("numMainThreadScrolls", numMainThreadScrolls);
    enumerator->AddInt64("numLayersDrawn", numLayersDrawn);
    enumerator->AddInt64("numMissingTiles", numMissingTiles);
    enumerator->AddInt64("totalDeferredImageDecodeCount",
                         totalDeferredImageDecodeCount);
    enumerator->AddInt64("totalDeferredImageCacheHitCount",
                         totalDeferredImageCacheHitCount);
    enumerator->AddInt64("totalImageGatheringCount", totalImageGatheringCount);
    enumerator->AddDouble("totalDeferredImageDecodeTimeInSeconds",
                          totalDeferredImageDecodeTime.InSecondsF());
    enumerator->AddDouble("totalImageGatheringTimeInSeconds",
                          totalImageGatheringTime.InSecondsF());
}

}  // namespace cc
