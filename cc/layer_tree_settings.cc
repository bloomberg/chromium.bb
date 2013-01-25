// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_settings.h"

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "cc/switches.h"

namespace cc {

LayerTreeSettings::LayerTreeSettings()
    : acceleratePainting(false)
    , compositorFrameMessage(false)
    , implSidePainting(false)
    , renderVSyncEnabled(true)
    , perTilePaintingEnabled(false)
    , partialSwapEnabled(false)
    , rightAlignedSchedulingEnabled(false)
    , acceleratedAnimationEnabled(true)
    , pageScalePinchZoomEnabled(false)
    , backgroundColorInsteadOfCheckerboard(false)
    , showOverdrawInTracing(false)
    , canUseLCDText(true)
    , shouldClearRootRenderPass(true)
    , useLinearFadeScrollbarAnimator(false)
    , calculateTopControlsPosition(false)
    , minimumContentsScale(0.0625f)
    , lowResContentsScaleFactor(0.125f)
    , refreshRate(0)
    , maxPartialTextureUpdates(std::numeric_limits<size_t>::max())
    , numRasterThreads(1)
    , topControlsHeightPx(0)
    , defaultTileSize(gfx::Size(256, 256))
    , maxUntiledLayerSize(gfx::Size(512, 512))
    , minimumOcclusionTrackingSize(gfx::Size(160, 160))
{
    // TODO(danakj): Move this to chromium when we don't go through the WebKit API anymore.
    compositorFrameMessage = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kEnableCompositorFrameMessage);
    partialSwapEnabled = CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePartialSwap);
    backgroundColorInsteadOfCheckerboard = CommandLine::ForCurrentProcess()->HasSwitch(switches::kBackgroundColorInsteadOfCheckerboard);
    showOverdrawInTracing = CommandLine::ForCurrentProcess()->HasSwitch(switches::kTraceOverdraw);

// TODO(alokp): Remove this hard-coded setting.
// Platforms that need to disable LCD text must explicitly set this value.
#if defined(OS_ANDROID)
    canUseLCDText = false;
#endif

#if defined(OS_ANDROID)
    // TODO(danakj): Move this out to the android code.
    maxPartialTextureUpdates = 0;
#endif

#if defined(OS_ANDROID)
    // TODO(danakj): Move this out to the android code.
    useLinearFadeScrollbarAnimator = true;
#endif

    initialDebugState.showPropertyChangedRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowPropertyChangedRects);
    initialDebugState.showSurfaceDamageRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowSurfaceDamageRects);
    initialDebugState.showScreenSpaceRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowScreenSpaceRects);
    initialDebugState.showReplicaScreenSpaceRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowReplicaScreenSpaceRects);
    initialDebugState.showOccludingRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowOccludingRects);
    initialDebugState.showNonOccludingRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowNonOccludingRects);
    if (CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kSlowDownRasterScaleFactor)) {
      int scaleAsInt;
      std::string value = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kSlowDownRasterScaleFactor);
      if (!base::StringToInt(value, &scaleAsInt))
          LOG(ERROR) << "Failed to parse the slow down raster scale factor:" << value;
      else
          initialDebugState.slowDownRasterScaleFactor = scaleAsInt;
    }

    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kNumRasterThreads)) {
        const size_t kMaxRasterThreads = 64;
        std::string num_raster_threads =
            CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                switches::kNumRasterThreads);
        int num_threads;
        if (base::StringToInt(num_raster_threads, &num_threads) &&
            num_threads > 0 && num_threads <= kMaxRasterThreads) {
            numRasterThreads = num_threads;
        } else {
            LOG(WARNING) << "Bad number of raster threads: " <<
                num_raster_threads;
        }
    }
}

LayerTreeSettings::~LayerTreeSettings()
{
}

}  // namespace cc
