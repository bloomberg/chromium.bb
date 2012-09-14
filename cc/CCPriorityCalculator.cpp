// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCPriorityCalculator.h"

using namespace std;

namespace cc {

// static
int CCPriorityCalculator::uiPriority(bool drawsToRootSurface)
{
    return drawsToRootSurface ? -1 : 2;
}

// static
int CCPriorityCalculator::visiblePriority(bool drawsToRootSurface)
{
    return drawsToRootSurface ? 0 : 3;
}

// static
int CCPriorityCalculator::renderSurfacePriority()
{
    return 1;
}

// static
int CCPriorityCalculator::lingeringPriority(int previousPriority)
{
    // FIXME: We should remove this once we have priorities for all
    //        textures (we can't currently calculate distances for
    //        off-screen textures).
    int lingeringPriority = 1000000;
    return min(numeric_limits<int>::max() - 1,
               max(lingeringPriority, previousPriority)) + 1;
}

namespace {
unsigned manhattanDistance(const IntRect& a, const IntRect& b)
{
    IntRect c = unionRect(a, b);
    int x = max(0, c.width() - a.width() - b.width() + 1);
    int y = max(0, c.height() - a.height() - b.height() + 1);
    return (x + y);
}
}

int CCPriorityCalculator::priorityFromDistance(const IntRect& visibleRect, const IntRect& textureRect, bool drawsToRootSurface) const
{
    unsigned distance = manhattanDistance(visibleRect, textureRect);
    if (!distance)
        return visiblePriority(drawsToRootSurface);
    return visiblePriority(false) + distance;
}

int CCPriorityCalculator::priorityFromDistance(unsigned pixels, bool drawsToRootSurface) const
{
    if (!pixels)
        return visiblePriority(drawsToRootSurface);
    return visiblePriority(false) + pixels;
}

int CCPriorityCalculator::priorityFromVisibility(bool visible, bool drawsToRootSurface) const
{
    return visible ? visiblePriority(drawsToRootSurface) : lowestPriority();
}

} // cc
