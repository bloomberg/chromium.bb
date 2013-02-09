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
    , useCheapnessEstimator(false)
    , minimumContentsScale(0.0625f)
    , lowResContentsScaleFactor(0.125f)
    , topControlsHeight(0.f)
    , refreshRate(0)
    , maxPartialTextureUpdates(std::numeric_limits<size_t>::max())
    , numRasterThreads(1)
    , defaultTileSize(gfx::Size(256, 256))
    , maxUntiledLayerSize(gfx::Size(512, 512))
    , minimumOcclusionTrackingSize(gfx::Size(160, 160))
{
}

LayerTreeSettings::~LayerTreeSettings()
{
}

}  // namespace cc
