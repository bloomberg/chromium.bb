// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCPriorityCalculator_h
#define CCPriorityCalculator_h

#include "IntRect.h"
#include "IntSize.h"

namespace cc {

class PriorityCalculator {
public:
    static int uiPriority(bool drawsToRootSurface);
    static int visiblePriority(bool drawsToRootSurface);
    static int renderSurfacePriority();
    static int lingeringPriority(int previousPriority);
    static int priorityFromDistance(const IntRect& visibleRect, const IntRect& textureRect, bool drawsToRootSurface);
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

#endif
