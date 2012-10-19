// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCPriorityCalculator.h"

using namespace std;

namespace cc {

static const int uiDrawsToRootSurfacePriority = -1;
static const int visibleDrawsToRootSurfacePriority = 0;
static const int renderSurfacesPriority = 1;
static const int uiDoesNotDrawToRootSurfacePriority = 2;
static const int visibleDoesNotDrawToRootSurfacePriority = 3;

// The lower digits are how far from being visible the texture is,
// in pixels.
static const int notVisibleBasePriority = 1000000;
static const int notVisibleLimitPriority = 1900000;

// Small animated layers are treated as though they are 512 pixels
// from being visible.
static const int smallAnimatedLayerPriority = notVisibleBasePriority + 512;

static const int lingeringBasePriority = 2000000;
static const int lingeringLimitPriority = 2900000;

// static
int CCPriorityCalculator::uiPriority(bool drawsToRootSurface)
{
    return drawsToRootSurface ? uiDrawsToRootSurfacePriority : uiDoesNotDrawToRootSurfacePriority;
}

// static
int CCPriorityCalculator::visiblePriority(bool drawsToRootSurface)
{
    return drawsToRootSurface ? visibleDrawsToRootSurfacePriority : visibleDoesNotDrawToRootSurfacePriority;
}

// static
int CCPriorityCalculator::renderSurfacePriority()
{
    return renderSurfacesPriority;
}

// static
int CCPriorityCalculator::lingeringPriority(int previousPriority)
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
int CCPriorityCalculator::priorityFromDistance(const IntRect& visibleRect, const IntRect& textureRect, bool drawsToRootSurface)
{
    int distance = manhattanDistance(visibleRect, textureRect);
    if (!distance)
        return visiblePriority(drawsToRootSurface);
    return min(notVisibleLimitPriority, notVisibleBasePriority + distance);
}

// static
int CCPriorityCalculator::smallAnimatedLayerMinPriority()
{
    return smallAnimatedLayerPriority;
}

} // cc
