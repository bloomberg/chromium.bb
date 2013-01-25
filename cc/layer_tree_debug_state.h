// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_TREE_DEBUG_STATE_H_
#define CC_LAYER_TREE_DEBUG_STATE_H_

#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT LayerTreeDebugState {
 public:
  LayerTreeDebugState();
  ~LayerTreeDebugState();

  bool showFPSCounter;
  bool showPlatformLayerTree;
  bool showDebugBorders;
  bool continuousPainting;

  bool showPaintRects;
  bool showPropertyChangedRects;
  bool showSurfaceDamageRects;
  bool showScreenSpaceRects;
  bool showReplicaScreenSpaceRects;
  bool showOccludingRects;
  bool showNonOccludingRects;

  int slowDownRasterScaleFactor;

  bool showHudInfo() const;
  bool showHudRects() const;
  bool hudNeedsFont() const;

  static bool equal(const LayerTreeDebugState& a, const LayerTreeDebugState& b);
  static LayerTreeDebugState unite(const LayerTreeDebugState& a, const LayerTreeDebugState& b);
};

}  // namespace cc

#endif  // CC_LAYER_TREE_DEBUG_STATE_H_
