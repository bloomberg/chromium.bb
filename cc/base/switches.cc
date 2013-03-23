// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/switches.h"

#include "base/command_line.h"

namespace cc {
namespace switches {

// On platforms where checkerboards are used, prefer background colors instead
// of checkerboards.
const char kBackgroundColorInsteadOfCheckerboard[] =
    "background-color-instead-of-checkerboard";

const char kDisableThreadedAnimation[] = "disable-threaded-animation";

// Send a message for every frame from the impl thread to the parent compositor.
const char kEnableCompositorFrameMessage[] = "enable-compositor-frame-message";

// Do not predict whether the tile will be either solid color or transparent.
const char kDisableColorEstimator[] = "disable-color-estimator";

// Paint content on the main thread instead of the compositor thread.
// Overrides the kEnableImplSidePainting flag.
const char kDisableImplSidePainting[] = "disable-impl-side-painting";

// Paint content on the compositor thread instead of the main thread.
const char kEnableImplSidePainting[] = "enable-impl-side-painting";

// Try to finish display pipeline before vsync tick
const char kEnableRightAlignedScheduling[] = "enable-right-aligned-scheduling";

const char kEnableTopControlsPositionCalculation[] =
    "enable-top-controls-position-calculation";

// Enable solid tile color, transparent tile, and cheapness prediction metrics.
const char kEnablePredictionBenchmarking[] = "enable-prediction-benchmarking";

// The height of the movable top controls.
const char kTopControlsHeight[] = "top-controls-height";

// Percentage of the top controls need to be hidden before they will auto hide.
const char kTopControlsHideThreshold[] = "top-controls-hide-threshold";

// Percentage of the top controls need to be shown before they will auto show.
const char kTopControlsShowThreshold[] = "top-controls-show-threshold";

// Number of worker threads used to rasterize content.
const char kNumRasterThreads[] = "num-raster-threads";

// Show metrics about overdraw in about:tracing recordings, such as the number
// of pixels culled, and the number of pixels drawn, for each frame.
const char kTraceOverdraw[] = "trace-overdraw";

// Logs all rendered frames.
const char kTraceAllRenderedFrames[] = "trace-all-rendered-frames";

// Re-rasters everything multiple times to simulate a much slower machine.
// Give a scale factor to cause raster to take that many times longer to
// complete, such as --slow-down-raster-scale-factor=25.
const char kSlowDownRasterScaleFactor[] = "slow-down-raster-scale-factor";

// Schedule rasterization jobs according to their estimated processing cost.
const char kUseCheapnessEstimator[] = "use-cheapness-estimator";

// The scale factor for low resolution tile contents.
const char kLowResolutionContentsScaleFactor[] =
    "low-resolution-contents-scale-factor";

// Causes the compositor to render to textures which are then sent to the parent
// through the texture mailbox mechanism.
// Requires --enable-compositor-frame-message.
const char kCompositeToMailbox[] = "composite-to-mailbox";

const char kEnablePartialSwap[] = "enable-partial-swap";
const char kUIEnablePartialSwap[] = "ui-enable-partial-swap";

const char kEnablePerTilePainting[] = "enable-per-tile-painting";
const char kUIEnablePerTilePainting[] = "ui-enable-per-tile-painting";

// Renders a border around compositor layers to help debug and study
// layer compositing.
const char kShowCompositedLayerBorders[] = "show-composited-layer-borders";
const char kUIShowCompositedLayerBorders[] = "ui-show-layer-borders";

// Draws a textual dump of the compositor layer tree to help debug and study
// layer compositing.
const char kShowCompositedLayerTree[] = "show-composited-layer-tree";
const char kUIShowCompositedLayerTree[] = "ui-show-layer-tree";

// Draws a FPS indicator
const char kShowFPSCounter[] = "show-fps-counter";
const char kUIShowFPSCounter[] = "ui-show-fps-counter";

// Show rects in the HUD around layers whose properties have changed.
const char kShowPropertyChangedRects[] = "show-property-changed-rects";
const char kUIShowPropertyChangedRects[] = "ui-show-property-changed-rects";

// Show rects in the HUD around damage as it is recorded into each render
// surface.
const char kShowSurfaceDamageRects[] = "show-surface-damage-rects";
const char kUIShowSurfaceDamageRects[] = "ui-show-surface-damage-rects";

// Show rects in the HUD around the screen-space transformed bounds of every
// layer.
const char kShowScreenSpaceRects[] = "show-screenspace-rects";
const char kUIShowScreenSpaceRects[] = "ui-show-screenspace-rects";

// Show rects in the HUD around the screen-space transformed bounds of every
// layer's replica, when they have one.
const char kShowReplicaScreenSpaceRects[] = "show-replica-screenspace-rects";
const char kUIShowReplicaScreenSpaceRects[] =
    "ui-show-replica-screenspace-rects";

// Show rects in the HUD wherever something is known to be drawn opaque and is
// considered occluding the pixels behind it.
const char kShowOccludingRects[] = "show-occluding-rects";
const char kUIShowOccludingRects[] = "ui-show-occluding-rects";

// Show rects in the HUD wherever something is not known to be drawn opaque and
// is not considered to be occluding the pixels behind it.
const char kShowNonOccludingRects[] = "show-nonoccluding-rects";
const char kUIShowNonOccludingRects[] = "ui-show-nonoccluding-rects";

bool IsImplSidePaintingEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(cc::switches::kDisableImplSidePainting))
    return false;
  else if (command_line.HasSwitch(cc::switches::kEnableImplSidePainting))
    return true;

#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

}  // namespace switches
}  // namespace cc
