// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/priority_calculator.h"

using namespace std;

namespace cc {

static const int nothingPriorityCutoff = -3;

static const int mostHighPriority = -2;

static const int uiDrawsToRootSurfacePriority = -1;
static const int visibleDrawsToRootSurfacePriority = 0;
static const int renderSurfacesPriority = 1;
static const int uiDoesNotDrawToRootSurfacePriority = 2;
static const int visibleDoesNotDrawToRootSurfacePriority = 3;

static const int visibleOnlyPriorityCutoff = 4;

// The lower digits are how far from being visible the texture is,
// in pixels.
static const int notVisibleBasePriority = 1000000;
static const int notVisibleLimitPriority = 1900000;

// Arbitrarily define "nearby" to be 2000 pixels. A better estimate
// would be percent-of-viewport or percent-of-screen.
static const int visibleAndNearbyPriorityCutoff = notVisibleBasePriority + 2000;

// Small animated layers are treated as though they are 512 pixels
// from being visible.
static const int smallAnimatedLayerPriority = notVisibleBasePriority + 512;

static const int lingeringBasePriority = 2000000;
static const int lingeringLimitPriority = 2900000;

static const int mostLowPriority = 3000000;

static const int everythingPriorityCutoff = 3000001;

// static
int PriorityCalculator::uiPriority(bool drawsToRootSurface)
{
    return drawsToRootSurface ? uiDrawsToRootSurfacePriority : uiDoesNotDrawToRootSurfacePriority;
}

// static
int PriorityCalculator::visiblePriority(bool drawsToRootSurface)
{
    return drawsToRootSurface ? visibleDrawsToRootSurfacePriority : visibleDoesNotDrawToRootSurfacePriority;
}

// static
int PriorityCalculator::renderSurfacePriority()
{
    return renderSurfacesPriority;
}

// static
int PriorityCalculator::lingeringPriority(int previousPriority)
{
    // FIXME: We should remove this once we have priorities for all
    //        textures (we can't currently calculate distances for
    //        off-screen textures).
    return min(lingeringLimitPriority,
               max(lingeringBasePriority, previousPriority + 1));
}

namespace {
int manhattanDistance(const IntRect& a, const IntRect& b)
{
    IntRect c = unionRect(a, b);
    int x = max(0, c.width() - a.width() - b.width() + 1);
    int y = max(0, c.height() - a.height() - b.height() + 1);
    return (x + y);
}
}

// static
int PriorityCalculator::priorityFromDistance(const IntRect& visibleRect, const IntRect& textureRect, bool drawsToRootSurface)
{
    int distance = manhattanDistance(visibleRect, textureRect);
    if (!distance)
        return visiblePriority(drawsToRootSurface);
    return min(notVisibleLimitPriority, notVisibleBasePriority + distance);
}

// static
int PriorityCalculator::smallAnimatedLayerMinPriority()
{
    return smallAnimatedLayerPriority;
}

// static 
int PriorityCalculator::highestPriority()
{
    return mostHighPriority;
}

// static
int PriorityCalculator::lowestPriority()
{
    return mostLowPriority;
}

// static
int PriorityCalculator::allowNothingCutoff()
{
    return nothingPriorityCutoff;
}

// static
int PriorityCalculator::allowVisibleOnlyCutoff()
{
    return visibleOnlyPriorityCutoff;
}

// static
int PriorityCalculator::allowVisibleAndNearbyCutoff()
{
    return visibleAndNearbyPriorityCutoff;
}

// static
int PriorityCalculator::allowEverythingCutoff()
{
    return everythingPriorityCutoff;
}

} // cc
