// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RENDERING_STATS_H_
#define CC_DEBUG_RENDERING_STATS_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "cc/base/cc_export.h"

namespace cc {

struct CC_EXPORT RenderingStats {
    // FIXME: Rename these to animationFrameCount and screenFrameCount, crbug.com/138641.
    int64 numAnimationFrames;
    int64 numFramesSentToScreen;
    int64 droppedFrameCount;
    base::TimeDelta totalPaintTime;
    base::TimeDelta totalRasterizeTime;
    base::TimeDelta totalRasterizeTimeForNowBinsOnPendingTree;
    base::TimeDelta totalCommitTime;
    int64 totalCommitCount;
    int64 totalPixelsPainted;
    int64 totalPixelsRasterized;
    int64 numImplThreadScrolls;
    int64 numMainThreadScrolls;
    int64 numLayersDrawn;
    int64 numMissingTiles;
    int64 totalDeferredImageDecodeCount;
    int64 totalDeferredImageCacheHitCount;
    int64 totalImageGatheringCount;
    base::TimeDelta totalDeferredImageDecodeTime;
    base::TimeDelta totalImageGatheringTime;
    // Note: when adding new members, please remember to update EnumerateFields
    // and Add in rendering_stats.cc.

    RenderingStats();

    // In conjunction with enumerateFields, this allows the embedder to
    // enumerate the values in this structure without
    // having to embed references to its specific member variables. This
    // simplifies the addition of new fields to this type.
    class Enumerator {
    public:
        virtual void AddInt64(const char* name, int64 value) = 0;
        virtual void AddDouble(const char* name, double value) = 0;
        virtual void AddInt(const char* name, int value) = 0;
        virtual void AddTimeDeltaInSecondsF(const char* name,
                                            const base::TimeDelta& value) = 0;

    protected:
        virtual ~Enumerator() { }
    };

    // Outputs the fields in this structure to the provided enumerator.
    void EnumerateFields(Enumerator* enumerator) const;

    // Add fields of |other| to the fields in this structure.
    void Add(const RenderingStats& other);
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_H_
