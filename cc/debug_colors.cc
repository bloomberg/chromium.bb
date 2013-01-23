// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug_colors.h"

#include "cc/layer_tree_impl.h"

namespace cc {

static const float Scale(float width, const LayerTreeImpl* tree_impl) {
  return width * (tree_impl ? tree_impl->device_scale_factor() : 1);
}

// ======= Layer border colors =======

// Tiled content layers are orange.
SkColor DebugColors::TiledContentLayerBorderColor() { return SkColorSetARGB(128, 255, 128, 0); }
int DebugColors::TiledContentLayerBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }

// Non-tiled content layers area green.
SkColor DebugColors::ContentLayerBorderColor() { return SkColorSetARGB(128, 0, 128, 32); }
int DebugColors::ContentLayerBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }

// Masking layers are pale blue and wide.
SkColor DebugColors::MaskingLayerBorderColor() { return SkColorSetARGB(48, 128, 255, 255); }
int DebugColors::MaskingLayerBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(20, tree_impl); }

// Other container layers are yellow.
SkColor DebugColors::ContainerLayerBorderColor() { return SkColorSetARGB(192, 255, 255, 0); }
int DebugColors::ContainerLayerBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }

// Render surfaces are blue.
SkColor DebugColors::SurfaceBorderColor() { return SkColorSetARGB(100, 0, 0, 255); }
int DebugColors::SurfaceBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }

// Replicas of render surfaces are purple.
SkColor DebugColors::SurfaceReplicaBorderColor() { return SkColorSetARGB(100, 160, 0, 255); }
int DebugColors::SurfaceReplicaBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }

// Tile borders are cyan.
SkColor DebugColors::TileBorderColor() { return SkColorSetARGB(100, 80, 200, 200); }
int DebugColors::TileBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(1, tree_impl); }

// ======= Tile colors =======

// Missing tile borders are red.
SkColor DebugColors::MissingTileBorderColor() { return SkColorSetARGB(100, 255, 0, 0); }
int DebugColors::MissingTileBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(1, tree_impl); }

// Culled tile borders are brown.
SkColor DebugColors::CulledTileBorderColor() { return SkColorSetARGB(120, 160, 100, 0); }
int DebugColors::CulledTileBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(1, tree_impl); }

// ======= Checkerboard colors =======

// Non-debug checkerboards are grey.
SkColor DebugColors::DefaultCheckerboardColor() { return SkColorSetRGB(241, 241, 241); }

// Invalidated tiles get sky blue checkerboards.
SkColor DebugColors::InvalidatedTileCheckerboardColor() { return SkColorSetRGB(128, 200, 245); }

// Evicted tiles get pale red checkerboards.
SkColor DebugColors::EvictedTileCheckerboardColor() { return SkColorSetRGB(255, 200, 200); }

// ======= Debug rect colors =======

// Paint rects in red.
SkColor DebugColors::PaintRectBorderColor() { return SkColorSetARGB(255, 255, 0, 0); }
int DebugColors::PaintRectBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }
SkColor DebugColors::PaintRectFillColor() { return SkColorSetARGB(30, 255, 0, 0); }

// Property-changed rects in blue.
SkColor DebugColors::PropertyChangedRectBorderColor() { return SkColorSetARGB(255, 0, 0, 255); }
int DebugColors::PropertyChangedRectBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }
SkColor DebugColors::PropertyChangedRectFillColor() { return SkColorSetARGB(30, 0, 0, 255); }

// Surface damage rects in yellow-orange.
SkColor DebugColors::SurfaceDamageRectBorderColor() { return SkColorSetARGB(255, 200, 100, 0); }
int DebugColors::SurfaceDamageRectBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }
SkColor DebugColors::SurfaceDamageRectFillColor() { return SkColorSetARGB(30, 200, 100, 0); }

// Surface replica screen space rects in green.
SkColor DebugColors::ScreenSpaceLayerRectBorderColor() { return SkColorSetARGB(255, 100, 200, 0); }
int DebugColors::ScreenSpaceLayerRectBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }
SkColor DebugColors::ScreenSpaceLayerRectFillColor() { return SkColorSetARGB(30, 100, 200, 0); }

// Layer screen space rects in purple.
SkColor DebugColors::ScreenSpaceSurfaceReplicaRectBorderColor() { return SkColorSetARGB(255, 100, 0, 200); }
int DebugColors::ScreenSpaceSurfaceReplicaRectBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }
SkColor DebugColors::ScreenSpaceSurfaceReplicaRectFillColor() { return SkColorSetARGB(10, 100, 0, 200); }

// Occluding rects in pink.
SkColor DebugColors::OccludingRectBorderColor() { return SkColorSetARGB(255, 245, 136, 255); }
int DebugColors::OccludingRectBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }
SkColor DebugColors::OccludingRectFillColor() { return SkColorSetARGB(10, 245, 136, 255); }

// Non-Occluding rects in a reddish color.
SkColor DebugColors::NonOccludingRectBorderColor() { return SkColorSetARGB(255, 200, 0, 100); }
int DebugColors::NonOccludingRectBorderWidth(const LayerTreeImpl* tree_impl) { return Scale(2, tree_impl); }
SkColor DebugColors::NonOccludingRectFillColor() { return SkColorSetARGB(10, 200, 0, 100); }

// ======= HUD widget colors =======

SkColor DebugColors::PlatformLayerTreeTextColor() { return SK_ColorRED; }
SkColor DebugColors::FPSDisplayTextAndGraphColor() { return SK_ColorRED; }

// Paint time display has the same green as used for paint time in the WebInspector
SkColor DebugColors::PaintTimeDisplayTextAndGraphColor() { return SkColorSetRGB(95, 160, 80); }

}  // namespace cc
