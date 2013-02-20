// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/switches.h"

namespace cc {
namespace switches {

// On platforms where checkerboards are used, prefer background colors instead
// of checkerboards.
const char kBackgroundColorInsteadOfCheckerboard[] =
    "background-color-instead-of-checkerboard";

const char kDisableThreadedAnimation[]      = "disable-threaded-animation";

// Send a message for every frame from the impl thread to the parent compositor.
const char kEnableCompositorFrameMessage[] = "enable-compositor-frame-message";

// Paint content on the compositor thread instead of the main thread.
const char kEnableImplSidePainting[] = "enable-impl-side-painting";

const char kEnablePartialSwap[]             = "enable-partial-swap";

const char kEnablePerTilePainting[]         = "enable-per-tile-painting";

// Try to finish display pipeline before vsync tick
const char kEnableRightAlignedScheduling[] = "enable-right-aligned-scheduling";

const char kEnableTopControlsPositionCalculation[] =
    "enable-top-controls-position-calculation";

// The height of the movable top controls.
const char kTopControlsHeight[] = "top-controls-height";

// Number of worker threads used to rasterize content.
const char kNumRasterThreads[] = "num-raster-threads";

// Show rects in the HUD around layers whose properties have changed.
const char kShowPropertyChangedRects[] = "show-property-changed-rects";

// Show rects in the HUD around damage as it is recorded into each render
// surface.
const char kShowSurfaceDamageRects[] = "show-surface-damage-rects";

// Show rects in the HUD around the screen-space transformed bounds of every
// layer.
const char kShowScreenSpaceRects[] = "show-screenspace-rects";

// Show rects in the HUD around the screen-space transformed bounds of every
// layer's replica, when they have one.
const char kShowReplicaScreenSpaceRects[] = "show-replica-screenspace-rects";

// Show rects in the HUD wherever something is known to be drawn opaque and is
// considered occluding the pixels behind it.
const char kShowOccludingRects[] = "show-occluding-rects";

// Show rects in the HUD wherever something is not known to be drawn opaque and
// is not considered to be occluding the pixels behind it.
const char kShowNonOccludingRects[] = "show-nonoccluding-rects";

// Show metrics about overdraw in about:tracing recordings, such as the number
// of pixels culled, and the number of pixels drawn, for each frame.
const char kTraceOverdraw[] = "trace-overdraw";

// Logs all rendered frames.
const char kTraceAllRenderedFrames[]      = "trace-all-rendered-frames";

// Re-rasters everything multiple times to simulate a much slower machine.
// Give a scale factor to cause raster to take that many times longer to
// complete, such as --slow-down-raster-scale-factor=25.
const char kSlowDownRasterScaleFactor[] = "slow-down-raster-scale-factor";

// Schedule rasterization jobs according to their estimated processing cost.
const char kUseCheapnessEstimator[] = "use-cheapness-estimator";

// The scale factor for low resolution tile contents.
const char kLowResolutionContentsScaleFactor[] =
    "low-resolution-contents-scale-factor";

}  // namespace switches
}  // namespace cc
