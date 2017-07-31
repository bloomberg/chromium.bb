// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/switches.h"

#include "base/command_line.h"

namespace cc {
namespace switches {

const char kDisableThreadedAnimation[] = "disable-threaded-animation";

// Disables layer-edge anti-aliasing in the compositor.
const char kDisableCompositedAntialiasing[] =
    "disable-composited-antialiasing";

// Disables sending the next BeginMainFrame before the previous commit
// activates. Overrides the kEnableMainFrameBeforeActivation flag.
const char kDisableMainFrameBeforeActivation[] =
    "disable-main-frame-before-activation";

// Enables sending the next BeginMainFrame before the previous commit activates.
const char kEnableMainFrameBeforeActivation[] =
    "enable-main-frame-before-activation";

// Enables defering image decodes to the image decode service.
const char kEnableCheckerImaging[] = "enable-checker-imaging";

// Disabled defering image decodes to the image decode service.
const char kDisableCheckerImaging[] = "disable-checker-imaging";

// Percentage of the browser controls need to be hidden before they will auto
// hide.
const char kBrowserControlsHideThreshold[] = "top-controls-hide-threshold";

// Percentage of the browser controls need to be shown before they will auto
// show.
const char kBrowserControlsShowThreshold[] = "top-controls-show-threshold";

// Re-rasters everything multiple times to simulate a much slower machine.
// Give a scale factor to cause raster to take that many times longer to
// complete, such as --slow-down-raster-scale-factor=25.
const char kSlowDownRasterScaleFactor[] = "slow-down-raster-scale-factor";

// Compress tile textures for GPUs supporting it.
const char kEnableTileCompression[] = "enable-tile-compression";

// Enables the GPU benchmarking extension
const char kEnableGpuBenchmarking[] = "enable-gpu-benchmarking";

// Effectively disables pipelining of compositor frame production stages by
// waiting for each stage to finish before completing a frame.
const char kRunAllCompositorStagesBeforeDraw[] =
    "run-all-compositor-stages-before-draw";

// Renders a border around compositor layers to help debug and study
// layer compositing.
const char kShowCompositedLayerBorders[] = "show-composited-layer-borders";
const char kUIShowCompositedLayerBorders[] = "ui-show-composited-layer-borders";
const char kCompositedRenderPassBorders[] = "renderpass";
const char kCompositedSurfaceBorders[] = "surface";
const char kCompositedLayerBorders[] = "layer";

// TODO(dcastagna): Draw debug quad borders only when it is actually
// an overlay candidate.
// Renders a border around GL composited overlay candidate quads to
// help debug and study overlay support.
const char kGlCompositedOverlayCandidateQuadBorder[] =
    "gl-composited-overlay-candidate-quad-border";

// Draws a heads-up-display showing Frames Per Second as well as GPU memory
// usage. If you also use --enable-logging=stderr --vmodule="head*=1" then FPS
// will also be output to the console log.
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

// Switches cc machinery to use layer lists instead of layer trees
const char kEnableLayerLists[] = "enable-layer-lists";
const char kUIEnableLayerLists[] = "ui-enable-layer-lists";

// Visualize overdraw by color-coding elements based on if they have other
// elements drawn underneath. This is good for showing where the UI might be
// doing more rendering work than necessary. The colors are hinting at the
// amount of overdraw on your screen for each pixel, as follows:
//
// True color: No overdraw.
// Blue: Overdrawn once.
// Green: Overdrawn twice.
// Pink: Overdrawn three times.
// Red: Overdrawn four or more times.
const char kShowOverdrawFeedback[] = "show-overdraw-feedback";

// Prevents the layer tree unit tests from timing out.
const char kCCLayerTreeTestNoTimeout[] = "cc-layer-tree-test-no-timeout";

// Increases timeout for memory checkers.
const char kCCLayerTreeTestLongTimeout[] = "cc-layer-tree-test-long-timeout";

// Makes pixel tests write their output instead of read it.
const char kCCRebaselinePixeltests[] = "cc-rebaseline-pixeltests";

// Disable re-use of non-exact resources to fulfill ResourcePool requests.
// Intended only for use in layout or pixel tests to reduce noise.
const char kDisallowNonExactResourceReuse[] =
    "disallow-non-exact-resource-reuse";

}  // namespace switches
}  // namespace cc
