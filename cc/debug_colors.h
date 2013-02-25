// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_COLORS_H_
#define CC_DEBUG_COLORS_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class LayerTreeImpl;

class DebugColors {
 public:
  static SkColor TiledContentLayerBorderColor();
  static int TiledContentLayerBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor ImageLayerBorderColor();
  static int ImageLayerBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor ContentLayerBorderColor();
  static int ContentLayerBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor MaskingLayerBorderColor();
  static int MaskingLayerBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor ContainerLayerBorderColor();
  static int ContainerLayerBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor SurfaceBorderColor();
  static int SurfaceBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor SurfaceReplicaBorderColor();
  static int SurfaceReplicaBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor HighResTileBorderColor();
  static int HighResTileBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor LowResTileBorderColor();
  static int LowResTileBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor ExtraHighResTileBorderColor();
  static int ExtraHighResTileBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor ExtraLowResTileBorderColor();
  static int ExtraLowResTileBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor MissingTileBorderColor();
  static int MissingTileBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor CulledTileBorderColor();
  static int CulledTileBorderWidth(const LayerTreeImpl* tree_impl);

  static SkColor DefaultCheckerboardColor();
  static SkColor EvictedTileCheckerboardColor();
  static SkColor InvalidatedTileCheckerboardColor();

  static SkColor PaintRectBorderColor();
  static int PaintRectBorderWidth(const LayerTreeImpl* tree_impl);
  static SkColor PaintRectFillColor();

  static SkColor PropertyChangedRectBorderColor();
  static int PropertyChangedRectBorderWidth(const LayerTreeImpl* tree_impl);
  static SkColor PropertyChangedRectFillColor();

  static SkColor SurfaceDamageRectBorderColor();
  static int SurfaceDamageRectBorderWidth(const LayerTreeImpl* tree_impl);
  static SkColor SurfaceDamageRectFillColor();

  static SkColor ScreenSpaceLayerRectBorderColor();
  static int ScreenSpaceLayerRectBorderWidth(const LayerTreeImpl* tree_impl);
  static SkColor ScreenSpaceLayerRectFillColor();

  static SkColor ScreenSpaceSurfaceReplicaRectBorderColor();
  static int ScreenSpaceSurfaceReplicaRectBorderWidth(const LayerTreeImpl* tree_impl);
  static SkColor ScreenSpaceSurfaceReplicaRectFillColor();

  static SkColor OccludingRectBorderColor();
  static int OccludingRectBorderWidth(const LayerTreeImpl* tree_impl);
  static SkColor OccludingRectFillColor();

  static SkColor NonOccludingRectBorderColor();
  static int NonOccludingRectBorderWidth(const LayerTreeImpl* tree_impl);
  static SkColor NonOccludingRectFillColor();

  static SkColor NonPaintedFillColor();
  static SkColor MissingPictureFillColor();

  static SkColor PlatformLayerTreeTextColor();
  static SkColor FPSDisplayTextAndGraphColor();
  static SkColor PaintTimeDisplayTextAndGraphColor();

  DISALLOW_IMPLICIT_CONSTRUCTORS(DebugColors);
};

}  // namespace cc

#endif  // CC_DEBUG_COLORS_H_
