// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCPriorityCalculator_h
#define CCPriorityCalculator_h

#include "GraphicsContext3D.h"
#include "IntRect.h"
#include "IntSize.h"

namespace cc {

class CCPriorityCalculator {
public:
    static int uiPriority(bool drawsToRootSurface);
    static int visiblePriority(bool drawsToRootSurface);
    static int renderSurfacePriority();
    static int lingeringPriority(int previousPriority);
    int priorityFromDistance(const IntRect& visibleRect, const IntRect& textureRect, bool drawsToRootSurface) const;
    int priorityFromDistance(unsigned pixels, bool drawsToRootSurface) const;
    int priorityFromVisibility(bool visible, bool drawsToRootSurface) const;

    static inline int highestPriority() { return std::numeric_limits<int>::min(); }
    static inline int lowestPriority() { return std::numeric_limits<int>::max(); }
    static inline bool priorityIsLower(int a, int b) { return a > b; }
    static inline bool priorityIsHigher(int a, int b) { return a < b; }
    static inline bool maxPriority(int a, int b) { return priorityIsHigher(a, b) ? a : b; }
};

}

#endif
