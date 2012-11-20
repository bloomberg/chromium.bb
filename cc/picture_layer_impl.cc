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
    LayerImpl(id) {
}

PictureLayerImpl::~PictureLayerImpl() {
}

const char* PictureLayerImpl::layerTypeAsString() const {
  return "PictureLayer";
}

void PictureLayerImpl::appendQuads(QuadSink& quadSink,
                                   AppendQuadsData& appendQuadsData) {

  const gfx::Rect& visible_rect = visibleContentRect();
  gfx::Rect content_rect(gfx::Point(), contentBounds());

  if (!tilings_.size())
    return;

  SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
  bool clipped = false;
  gfx::QuadF target_quad = MathUtil::mapQuad(
      drawTransform(),
      gfx::QuadF(visible_rect),
      clipped);
  bool isAxisAlignedInTarget = !clipped && target_quad.IsRectilinear();
  bool useAA = !isAxisAlignedInTarget;

  // TODO(enne): Generate quads from multiple tilings.
  PictureLayerTiling* tiling = tilings_[0];
  for (PictureLayerTiling::Iterator iter(tiling, visible_rect); iter; ++iter) {
    ResourceProvider::ResourceId resource;
    if (*iter)
      resource = iter->resource_id();

    if (!resource) {
      // TODO(enne): draw checkerboards, etc...
      continue;
    }

    gfx::Rect geometry_rect = iter.geometry_rect();
    gfx::RectF texture_rect = iter.texture_rect();
    gfx::Rect opaque_rect = iter.opaque_rect();

    bool outside_left_edge = geometry_rect.x() == content_rect.x();
    bool outside_top_edge = geometry_rect.y() == content_rect.y();
    bool outside_right_edge = geometry_rect.right() == content_rect.right();
    bool outside_bottom_edge = geometry_rect.bottom() == content_rect.bottom();

    quadSink.append(TileDrawQuad::create(
        sharedQuadState,
        geometry_rect,
        opaque_rect,
        resource,
        texture_rect,
        iter.texture_size(),
        iter->contents_swizzled(),
        outside_left_edge && useAA,
        outside_top_edge && useAA,
        outside_right_edge && useAA,
        outside_bottom_edge && useAA).PassAs<DrawQuad>(), appendQuadsData);
  }
}

void PictureLayerImpl::dumpLayerProperties(std::string*, int indent) const {
  // TODO(enne): implement me
}

scoped_refptr<Tile> PictureLayerImpl::CreateTile(PictureLayerTiling*,
                                                 gfx::Rect rect) {
  // TODO(nduca): where does this come from?
  TileManager* tile_manager = NULL;

  return make_scoped_refptr(new Tile(
      tile_manager,
      &pile_,
      rect.size(),
      GL_RGBA,
      rect));
}

void PictureLayerImpl::SyncFromActiveLayer(const PictureLayerImpl* other) {
  tilings_.clear();
  for (size_t i = 0; i < other->tilings_.size(); ++i) {
    scoped_ptr<PictureLayerTiling> clone = other->tilings_[i]->Clone();
    clone->set_client(this);
    tilings_.append(clone.Pass());
  }
}

void PictureLayerImpl::Update() {
  // TODO(enne): Add more tilings during pinch zoom.
  if (!tilings_.size()) {
    gfx::Size tile_size = layerTreeHostImpl()->settings().defaultTileSize;

    scoped_ptr<PictureLayerTiling> tiling = PictureLayerTiling::Create(
        tile_size);
    tiling->set_client(this);
    tiling->SetBounds(contentBounds());
    tiling->create_tiles(gfx::Rect(gfx::Point(), contentBounds()));
    tilings_.append(tiling.Pass());

    // TODO(enne): handle invalidations, create new tiles
  }
}

}  // namespace cc
