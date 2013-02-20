// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_TREE_SETTINGS_H_
#define CC_LAYER_TREE_SETTINGS_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/layer_tree_debug_state.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT LayerTreeSettings {
 public:
  LayerTreeSettings();
  ~LayerTreeSettings();

  bool acceleratePainting;
  bool compositorFrameMessage;
  bool implSidePainting;
  bool renderVSyncEnabled;
  bool perTilePaintingEnabled;
  bool partialSwapEnabled;
  bool cacheRenderPassContents;
  bool rightAlignedSchedulingEnabled;
  bool acceleratedAnimationEnabled;
  bool pageScalePinchZoomEnabled;
  bool backgroundColorInsteadOfCheckerboard;
  bool showOverdrawInTracing;
  bool canUseLCDText;
  bool shouldClearRootRenderPass;
  bool useLinearFadeScrollbarAnimator;
  bool solidColorScrollbars;
  SkColor solidColorScrollbarColor;
  int solidColorScrollbarThicknessDIP;
  bool calculateTopControlsPosition;
  bool useCheapnessEstimator;
  bool useMemoryManagement;
  float minimumContentsScale;
  float lowResContentsScaleFactor;
  float topControlsHeight;
  double refreshRate;
  size_t maxPartialTextureUpdates;
  size_t numRasterThreads;
  gfx::Size defaultTileSize;
  gfx::Size maxUntiledLayerSize;
  gfx::Size minimumOcclusionTrackingSize;

  LayerTreeDebugState initialDebugState;
};

}  // namespace cc

#endif  // CC_LAYER_TREE_SETTINGS_H_
