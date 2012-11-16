// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug_colors.h"

#include "cc/layer_tree_host_impl.h"

namespace cc {

static const float Scale(float width, const LayerTreeHostImpl* host_impl) {
  return width * (host_impl ? host_impl->deviceScaleFactor() : 1);
}

// Tiled content layers are orange.
SkColor DebugColors::TiledContentLayerBorderColor() { return SkColorSetARGB(128, 255, 128, 0); }
int DebugColors::TiledContentLayerBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(2, host_impl); }

// Non-tiled content layers area green.
SkColor DebugColors::ContentLayerBorderColor() { return SkColorSetARGB(128, 0, 128, 32); }
int DebugColors::ContentLayerBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(2, host_impl); }

// Masking layers are pale blue and wide.
SkColor DebugColors::MaskingLayerBorderColor() { return SkColorSetARGB(48, 128, 255, 255); }
int DebugColors::MaskingLayerBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(20, host_impl); }

// Other container layers are yellow.
SkColor DebugColors::ContainerLayerBorderColor() { return SkColorSetARGB(192, 255, 255, 0); }
int DebugColors::ContainerLayerBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(2, host_impl); }

// Render surfaces are blue.
SkColor DebugColors::SurfaceBorderColor() { return SkColorSetARGB(100, 0, 0, 255); }
int DebugColors::SurfaceBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(2, host_impl); }

// Replicas of render surfaces are purple.
SkColor DebugColors::SurfaceReplicaBorderColor() { return SkColorSetARGB(100, 160, 0, 255); }
int DebugColors::SurfaceReplicaBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(2, host_impl); }

// Tile borders are cyan.
SkColor DebugColors::TileBorderColor() { return SkColorSetARGB(100, 80, 200, 200); }
int DebugColors::TileBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(1, host_impl); }

// Missing tile borders are red.
SkColor DebugColors::MissingTileBorderColor() { return SkColorSetARGB(100, 255, 0, 0); }
int DebugColors::MissingTileBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(1, host_impl); }

// Culled tile borders are brown.
SkColor DebugColors::CulledTileBorderColor() { return SkColorSetARGB(120, 160, 100, 0); }
int DebugColors::CulledTileBorderWidth(const LayerTreeHostImpl* host_impl) { return Scale(1, host_impl); }

// Invalidated tiles get sky blue checkerboards.
SkColor DebugColors::InvalidatedTileCheckerboardColor() { return SkColorSetRGB(128, 200, 245); }

// Evicted tiles get pale red checkerboards.
SkColor DebugColors::EvictedTileCheckerboardColor() { return SkColorSetRGB(255, 200, 200); }

}  // namespace cc
