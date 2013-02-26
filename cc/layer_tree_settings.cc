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
    , cacheRenderPassContents(true)
    , rightAlignedSchedulingEnabled(false)
    , acceleratedAnimationEnabled(true)
    , backgroundColorInsteadOfCheckerboard(false)
    , showOverdrawInTracing(false)
    , canUseLCDText(true)
    , shouldClearRootRenderPass(true)
    , useLinearFadeScrollbarAnimator(false)
    , solidColorScrollbars(false)
    , solidColorScrollbarColor(SK_ColorWHITE)
    , solidColorScrollbarThicknessDIP(-1)
    , calculateTopControlsPosition(false)
    , useCheapnessEstimator(false)
    , useMemoryManagement(true)
    , minimumContentsScale(0.0625f)
    , lowResContentsScaleFactor(0.125f)
    , topControlsHeight(0.f)
    , topControlsShowThreshold(0.5f)
    , topControlsHideThreshold(0.5f)
    , refreshRate(0)
    , maxPartialTextureUpdates(std::numeric_limits<size_t>::max())
    , numRasterThreads(1)
    , defaultTileSize(gfx::Size(256, 256))
    , maxUntiledLayerSize(gfx::Size(512, 512))
    , minimumOcclusionTrackingSize(gfx::Size(160, 160))
{
    // TODO(danakj): Renable surface caching when we can do it more realiably. crbug.com/170713
    cacheRenderPassContents = false;
}

LayerTreeSettings::~LayerTreeSettings()
{
}

}  // namespace cc
