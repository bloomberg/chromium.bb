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

// Disables LCD text.
const char kDisableLCDText[] = "disable-lcd-text";

const char kDisableThreadedAnimation[] = "disable-threaded-animation";

// Disables layer-edge anti-aliasing in the compositor.
const char kDisableCompositedAntialiasing[] =
    "disable-composited-antialiasing";

// Paint content on the main thread instead of the compositor thread.
// Overrides the kEnableImplSidePainting flag.
const char kDisableImplSidePainting[] = "disable-impl-side-painting";

// Enables LCD text.
const char kEnableLCDText[] = "enable-lcd-text";

// Paint content on the compositor thread instead of the main thread.
const char kEnableImplSidePainting[] = "enable-impl-side-painting";

const char kEnableTopControlsPositionCalculation[] =
    "enable-top-controls-position-calculation";

// Allow heuristics to determine when a layer tile should be drawn with
// the Skia GPU backend.  Only valid with GPU accelerated compositing +
// impl-side painting.
const char kEnableGPURasterization[] = "enable-gpu-rasterization";

// Disable GPU rasterization, i.e. rasterize on the CPU only.
// Overrides the kEnableGPURasterization flag.
const char kDisableGPURasterization[] = "disable-gpu-rasterization";

// The height of the movable top controls.
const char kTopControlsHeight[] = "top-controls-height";

// Percentage of the top controls need to be hidden before they will auto hide.
const char kTopControlsHideThreshold[] = "top-controls-hide-threshold";

// Percentage of the top controls need to be shown before they will auto show.
const char kTopControlsShowThreshold[] = "top-controls-show-threshold";

// Show metrics about overdraw in about:tracing recordings, such as the number
// of pixels culled, and the number of pixels drawn, for each frame.
const char kTraceOverdraw[] = "trace-overdraw";

// Re-rasters everything multiple times to simulate a much slower machine.
// Give a scale factor to cause raster to take that many times longer to
// complete, such as --slow-down-raster-scale-factor=25.
const char kSlowDownRasterScaleFactor[] = "slow-down-raster-scale-factor";

// Max tiles allowed for each tilings interest area.
const char kMaxTilesForInterestArea[] = "max-tiles-for-interest-area";

// The amount of unused resource memory compositor is allowed to keep around.
const char kMaxUnusedResourceMemoryUsagePercentage[] =
    "max-unused-resource-memory-usage-percentage";

// Causes the compositor to render to textures which are then sent to the parent
// through the texture mailbox mechanism.
// Requires --enable-compositor-frame-message.
const char kCompositeToMailbox[] = "composite-to-mailbox";

// Check that property changes during paint do not occur.
const char kStrictLayerPropertyChangeChecking[] =
    "strict-layer-property-change-checking";

// Virtual viewport for fixed-position elements, scrollbars during pinch.
const char kEnablePinchVirtualViewport[] = "enable-pinch-virtual-viewport";

// Disable partial swap which is needed for some OpenGL drivers / emulators.
const char kUIDisablePartialSwap[] = "ui-disable-partial-swap";

const char kEnablePerTilePainting[] = "enable-per-tile-painting";
const char kUIEnablePerTilePainting[] = "ui-enable-per-tile-painting";

// Enables the GPU benchmarking extension
const char kEnableGpuBenchmarking[] = "enable-gpu-benchmarking";

// Renders a border around compositor layers to help debug and study
// layer compositing.
const char kShowCompositedLayerBorders[] = "show-composited-layer-borders";
const char kUIShowCompositedLayerBorders[] = "ui-show-layer-borders";

// Draws a FPS indicator
const char kShowFPSCounter[] = "show-fps-counter";
const char kUIShowFPSCounter[] = "ui-show-fps-counter";

// Renders a border that represents the bounding box for the layer's animation.
const char kShowLayerAnimationBounds[] = "show-layer-animation-bounds";
const char kUIShowLayerAnimationBounds[] = "ui-show-layer-animation-bounds";

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

// Enable rasterizer that writes directly to GPU memory.
const char kEnableMapImage[] = "enable-map-image";

// Disable rasterizer that writes directly to GPU memory.
// Overrides the kEnableMapImage flag.
const char kDisableMapImage[] = "disable-map-image";

// Prevents the layer tree unit tests from timing out.
const char kCCLayerTreeTestNoTimeout[] = "cc-layer-tree-test-no-timeout";

// Makes pixel tests write their output instead of read it.
const char kCCRebaselinePixeltests[] = "cc-rebaseline-pixeltests";

// Disable textures using RGBA_4444 layout.
const char kDisable4444Textures[] = "disable-4444-textures";

// Disable touch hit testing in the compositor.
const char kDisableCompositorTouchHitTesting[] =
    "disable-compositor-touch-hit-testing";

bool IsLCDTextEnabled() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableLCDText))
    return false;
  else if (command_line->HasSwitch(switches::kEnableLCDText))
    return true;

#if defined(OS_ANDROID)
  return false;
#else
  return true;
#endif
}

bool IsGpuRasterizationEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableGPURasterization))
    return false;
  else if (command_line.HasSwitch(switches::kEnableGPURasterization))
    return true;

  return false;
}

bool IsImplSidePaintingEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableImplSidePainting))
    return false;
  else if (command_line.HasSwitch(switches::kEnableImplSidePainting))
    return true;

#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

bool IsMapImageEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableMapImage))
    return false;
  else if (command_line.HasSwitch(switches::kEnableMapImage))
    return true;

  return false;
}

}  // namespace switches
}  // namespace cc
