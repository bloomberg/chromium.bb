// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PRIORITY_CALCULATOR_H_
#define CC_PRIORITY_CALCULATOR_H_

#include "cc/cc_export.h"

namespace gfx {
class Rect;
}

namespace cc {

class CC_EXPORT PriorityCalculator {
public:
    static int uiPriority(bool drawsToRootSurface);
    static int visiblePriority(bool drawsToRootSurface);
    static int renderSurfacePriority();
    static int lingeringPriority(int previousPriority);
    static int priorityFromDistance(const gfx::Rect& visibleRect, const gfx::Rect& textureRect, bool drawsToRootSurface);
    static int smallAnimatedLayerMinPriority();

    static int highestPriority();
    static int lowestPriority();
    static inline bool priorityIsLower(int a, int b) { return a > b; }
    static inline bool priorityIsHigher(int a, int b) { return a < b; }
    static inline int maxPriority(int a, int b) { return priorityIsHigher(a, b) ? a : b; }

    static int allowNothingCutoff();
    static int allowVisibleOnlyCutoff();
    static int allowVisibleAndNearbyCutoff();
    static int allowEverythingCutoff();
};

}

#endif  // CC_PRIORITY_CALCULATOR_H_
