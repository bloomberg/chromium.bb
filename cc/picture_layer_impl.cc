// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_impl.h"

#include "cc/layer_tree_host_impl.h"
#include "cc/math_util.h"
#include "cc/quad_sink.h"
#include "cc/tile_draw_quad.h"

namespace cc {

PictureLayerImpl::PictureLayerImpl(int id) :
    LayerImpl(id),
    tilings_(this) {
}

PictureLayerImpl::~PictureLayerImpl() {
}

const char* PictureLayerImpl::layerTypeAsString() const {
  return "PictureLayer";
}

void PictureLayerImpl::appendQuads(QuadSink& quadSink,
                                   AppendQuadsData& appendQuadsData) {

  const gfx::Rect& rect = visibleContentRect();
  gfx::Rect content_rect(gfx::Point(), contentBounds());

  SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
  bool clipped = false;
  gfx::QuadF target_quad = MathUtil::mapQuad(
      drawTransform(),
      gfx::QuadF(rect),
      clipped);
  bool isAxisAlignedInTarget = !clipped && target_quad.IsRectilinear();
  bool useAA = !isAxisAlignedInTarget;

  for (PictureLayerTilingSet::Iterator iter(&tilings_, contentsScaleX(), rect);
       iter;
       ++iter) {
    ResourceProvider::ResourceId resource;
    if (*iter)
      resource = iter->resource_id();

    if (!resource) {
      // TODO(enne): draw checkerboards, etc...
      continue;
    }

    gfx::Rect geometry_rect = iter.geometry_rect();
    gfx::RectF texture_rect = iter.texture_rect();
    gfx::Rect opaque_rect = iter->opaque_rect();
    opaque_rect.Intersect(content_rect);

    bool outside_left_edge = geometry_rect.x() == content_rect.x();
    bool outside_top_edge = geometry_rect.y() == content_rect.y();
    bool outside_right_edge = geometry_rect.right() == content_rect.right();
    bool outside_bottom_edge = geometry_rect.bottom() == content_rect.bottom();

    scoped_ptr<TileDrawQuad> quad = TileDrawQuad::Create();
    quad->SetNew(sharedQuadState,
                 geometry_rect,
                 opaque_rect,
                 resource,
                 texture_rect,
                 iter.texture_size(),
                 iter->contents_swizzled(),
                 outside_left_edge && useAA,
                 outside_top_edge && useAA,
                 outside_right_edge && useAA,
                 outside_bottom_edge && useAA);
    quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
  }
}

void PictureLayerImpl::dumpLayerProperties(std::string*, int indent) const {
  // TODO(enne): implement me
}

void PictureLayerImpl::didUpdateTransforms() {
  tilings_.SetLayerBounds(bounds());
  // TODO(enne): Add more tilings during pinch zoom.
  if (!tilings_.num_tilings()) {
    gfx::Size tile_size = layerTreeHostImpl()->settings().defaultTileSize;
    tilings_.AddTiling(contentsScaleX(), tile_size);
    // TODO(enne): handle invalidations, create new tiles
  }
}

scoped_refptr<Tile> PictureLayerImpl::CreateTile(PictureLayerTiling*,
                                                 gfx::Rect rect) {
  TileManager* tile_manager = layerTreeHostImpl()->tileManager();

  return make_scoped_refptr(new Tile(
      tile_manager,
      &pile_,
      rect.size(),
      GL_RGBA,
      rect));
}

void PictureLayerImpl::SyncFromActiveLayer(const PictureLayerImpl* other) {
  tilings_.CloneFrom(other->tilings_);
}

}  // namespace cc
