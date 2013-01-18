// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_debug_state.h"

#include "base/logging.h"

namespace cc {

LayerTreeDebugState::LayerTreeDebugState()
  : showFPSCounter(false)
  , showPlatformLayerTree(false)
  , showDebugBorders(false)
  , continuousPainting(false)
  , showPaintRects(false)
  , showPropertyChangedRects(false)
  , showSurfaceDamageRects(false)
  , showScreenSpaceRects(false)
  , showReplicaScreenSpaceRects(false)
  , showOccludingRects(false)
  , showNonOccludingRects(false) { }

LayerTreeDebugState::~LayerTreeDebugState() {
}

bool LayerTreeDebugState::showHudInfo() const {
    return showFPSCounter || showPlatformLayerTree || continuousPainting || showHudRects();
}

bool LayerTreeDebugState::showHudRects() const {
    return showPaintRects || showPropertyChangedRects || showSurfaceDamageRects || showScreenSpaceRects || showReplicaScreenSpaceRects || showOccludingRects || showNonOccludingRects;
}

bool LayerTreeDebugState::hudNeedsFont() const {
    return showFPSCounter || showPlatformLayerTree || continuousPainting;
}

bool LayerTreeDebugState::equal(const LayerTreeDebugState& a, const LayerTreeDebugState& b) {
    return memcmp(&a, &b, sizeof(LayerTreeDebugState)) == 0;
}

LayerTreeDebugState LayerTreeDebugState::unite(const LayerTreeDebugState& a, const LayerTreeDebugState& b) {
    LayerTreeDebugState r(a);

    r.showFPSCounter |= b.showFPSCounter;
    r.showPlatformLayerTree |= b.showPlatformLayerTree;
    r.showDebugBorders |= b.showDebugBorders;
    r.continuousPainting |= b.continuousPainting;

    r.showPaintRects |= b.showPaintRects;
    r.showPropertyChangedRects |= b.showPropertyChangedRects;
    r.showSurfaceDamageRects |= b.showSurfaceDamageRects;
    r.showScreenSpaceRects |= b.showScreenSpaceRects;
    r.showReplicaScreenSpaceRects |= b.showReplicaScreenSpaceRects;
    r.showOccludingRects |= b.showOccludingRects;
    r.showNonOccludingRects |= b.showNonOccludingRects;

    return r;
}

}  // namespace cc
