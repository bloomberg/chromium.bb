// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiled_layer_impl.h"

#include "base/basictypes.h"
#include "base/stringprintf.h"
#include "cc/append_quads_data.h"
#include "cc/checkerboard_draw_quad.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/debug_colors.h"
#include "cc/layer_tiling_data.h"
#include "cc/math_util.h"
#include "cc/quad_sink.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/quad_f.h"

namespace cc {

// Temporary diagnostic.
static bool s_safe_to_delete_drawable_tile = false;

class DrawableTile : public LayerTilingData::Tile {
 public:
  static scoped_ptr<DrawableTile> Create() {
    return make_scoped_ptr(new DrawableTile());
  }

  virtual ~DrawableTile() { CHECK(s_safe_to_delete_drawable_tile); }

  ResourceProvider::ResourceId resource_id() const { return resource_id_; }
  void set_resource_id(ResourceProvider::ResourceId resource_id) {
    resource_id_ = resource_id;
  }
  bool contents_swizzled() { return contents_swizzled_; }
  void set_contents_swizzled(bool contents_swizzled) {
    contents_swizzled_ = contents_swizzled;
  }

 private:
  DrawableTile() : resource_id_(0), contents_swizzled_(false) {}

  ResourceProvider::ResourceId resource_id_;
  bool contents_swizzled_;

  DISALLOW_COPY_AND_ASSIGN(DrawableTile);
};

TiledLayerImpl::TiledLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id), skips_draw_(true) {}

TiledLayerImpl::~TiledLayerImpl() {
  s_safe_to_delete_drawable_tile = true;
  if (tiler_)
    tiler_->reset();
  s_safe_to_delete_drawable_tile = false;
}

ResourceProvider::ResourceId TiledLayerImpl::ContentsResourceId() const {
  // This function is only valid for single texture layers, e.g. masks.
  DCHECK(tiler_);
  DCHECK(tiler_->numTilesX() == 1);
  DCHECK(tiler_->numTilesY() == 1);

  DrawableTile* tile = TileAt(0, 0);
  ResourceProvider::ResourceId resource_id = tile ? tile->resource_id() : 0;
  return resource_id;
}

void TiledLayerImpl::DumpLayerProperties(std::string* str, int indent) const {
  str->append(IndentString(indent));
  base::StringAppendF(str, "skipsDraw: %d\n", (!tiler_ || skips_draw_));
  LayerImpl::DumpLayerProperties(str, indent);
}

bool TiledLayerImpl::HasTileAt(int i, int j) const {
  return tiler_->tileAt(i, j);
}

bool TiledLayerImpl::HasResourceIdForTileAt(int i, int j) const {
  return HasTileAt(i, j) && TileAt(i, j)->resource_id();
}

DrawableTile* TiledLayerImpl::TileAt(int i, int j) const {
  return static_cast<DrawableTile*>(tiler_->tileAt(i, j));
}

DrawableTile* TiledLayerImpl::CreateTile(int i, int j) {
  scoped_ptr<DrawableTile> tile(DrawableTile::Create());
  DrawableTile* added_tile = tile.get();
  tiler_->addTile(tile.PassAs<LayerTilingData::Tile>(), i, j);

  // Temporary diagnostic checks.
  CHECK(added_tile);
  CHECK(TileAt(i, j));

  return added_tile;
}

void TiledLayerImpl::GetDebugBorderProperties(SkColor* color,
                                              float* width) const {
  *color = DebugColors::TiledContentLayerBorderColor();
  *width = DebugColors::TiledContentLayerBorderWidth(layer_tree_impl());
}

scoped_ptr<LayerImpl> TiledLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return TiledLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void TiledLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  TiledLayerImpl* tiled_layer = static_cast<TiledLayerImpl*>(layer);

  tiled_layer->set_skips_draw(skips_draw_);
  tiled_layer->SetTilingData(*tiler_);

  for (LayerTilingData::TileMap::const_iterator iter = tiler_->tiles().begin();
       iter != tiler_->tiles().end();
       ++iter) {
    int i = iter->first.first;
    int j = iter->first.second;
    DrawableTile* tile = static_cast<DrawableTile*>(iter->second);

    tiled_layer->PushTileProperties(i,
                                    j,
                                    tile->resource_id(),
                                    tile->opaqueRect(),
                                    tile->contents_swizzled());
  }
}

void TiledLayerImpl::AppendQuads(QuadSink* quad_sink,
                                 AppendQuadsData* append_quads_data) {
  gfx::Rect content_rect = visible_content_rect();

  if (!tiler_ || tiler_->hasEmptyBounds() || content_rect.IsEmpty())
    return;

  SharedQuadState* shared_quad_state =
      quad_sink->useSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  int left, top, right, bottom;
  tiler_->contentRectToTileIndices(content_rect, left, top, right, bottom);

  if (ShowDebugBorders()) {
    for (int j = top; j <= bottom; ++j) {
      for (int i = left; i <= right; ++i) {
        DrawableTile* tile = TileAt(i, j);
        gfx::Rect tile_rect = tiler_->tileBounds(i, j);
        SkColor border_color;
        float border_width;

        if (skips_draw_ || !tile || !tile->resource_id()) {
          border_color = DebugColors::MissingTileBorderColor();
          border_width = DebugColors::MissingTileBorderWidth(layer_tree_impl());
        } else {
          border_color = DebugColors::HighResTileBorderColor();
          border_width = DebugColors::HighResTileBorderWidth(layer_tree_impl());
        }
        scoped_ptr<DebugBorderDrawQuad> debug_border_quad =
            DebugBorderDrawQuad::Create();
        debug_border_quad->SetNew(
            shared_quad_state, tile_rect, border_color, border_width);
        quad_sink->append(debug_border_quad.PassAs<DrawQuad>(),
                          append_quads_data);
      }
    }
  }

  if (skips_draw_)
    return;

  for (int j = top; j <= bottom; ++j) {
    for (int i = left; i <= right; ++i) {
      DrawableTile* tile = TileAt(i, j);
      gfx::Rect tile_rect = tiler_->tileBounds(i, j);
      gfx::Rect display_rect = tile_rect;
      tile_rect.Intersect(content_rect);

      // Skip empty tiles.
      if (tile_rect.IsEmpty())
        continue;

      if (!tile || !tile->resource_id()) {
        if (DrawCheckerboardForMissingTiles()) {
          SkColor checker_color;
          if (ShowDebugBorders()) {
            checker_color =
                tile ? DebugColors::InvalidatedTileCheckerboardColor()
                     : DebugColors::EvictedTileCheckerboardColor();
          } else {
            checker_color = DebugColors::DefaultCheckerboardColor();
          }

          scoped_ptr<CheckerboardDrawQuad> checkerboard_quad =
              CheckerboardDrawQuad::Create();
          checkerboard_quad->SetNew(
              shared_quad_state, tile_rect, checker_color);
          if (quad_sink->append(checkerboard_quad.PassAs<DrawQuad>(),
                                append_quads_data))
            append_quads_data->numMissingTiles++;
        } else {
          scoped_ptr<SolidColorDrawQuad> solid_color_quad =
              SolidColorDrawQuad::Create();
          solid_color_quad->SetNew(
              shared_quad_state, tile_rect, background_color());
          if (quad_sink->append(solid_color_quad.PassAs<DrawQuad>(),
                                append_quads_data))
            append_quads_data->numMissingTiles++;
        }
        continue;
      }

      gfx::Rect tile_opaque_rect = contents_opaque() ? tile_rect :
        gfx::IntersectRects(tile->opaqueRect(), content_rect);

      // Keep track of how the top left has moved, so the texture can be
      // offset the same amount.
      gfx::Vector2d display_offset = tile_rect.origin() - display_rect.origin();
      gfx::Vector2d texture_offset =
          tiler_->textureOffset(i, j) + display_offset;
      gfx::RectF tex_coord_rect = gfx::RectF(tile_rect.size()) + texture_offset;

      float tile_width = static_cast<float>(tiler_->tileSize().width());
      float tile_height = static_cast<float>(tiler_->tileSize().height());
      gfx::Size texture_size(tile_width, tile_height);

      scoped_ptr<TileDrawQuad> quad = TileDrawQuad::Create();
      quad->SetNew(shared_quad_state,
                   tile_rect,
                   tile_opaque_rect,
                   tile->resource_id(),
                   tex_coord_rect,
                   texture_size,
                   tile->contents_swizzled());
      quad_sink->append(quad.PassAs<DrawQuad>(), append_quads_data);
    }
  }
}

void TiledLayerImpl::SetTilingData(const LayerTilingData& tiler) {
  s_safe_to_delete_drawable_tile = true;

  if (tiler_) {
    tiler_->reset();
  } else {
    tiler_ = LayerTilingData::create(tiler.tileSize(),
                                     tiler.hasBorderTexels()
                                         ? LayerTilingData::HasBorderTexels
                                         : LayerTilingData::NoBorderTexels);
  }
  *tiler_ = tiler;

  s_safe_to_delete_drawable_tile = false;
}

void TiledLayerImpl::PushTileProperties(
    int i,
    int j,
    ResourceProvider::ResourceId resource_id,
    gfx::Rect opaque_rect,
    bool contents_swizzled) {
  DrawableTile* tile = TileAt(i, j);
  if (!tile)
    tile = CreateTile(i, j);
  tile->set_resource_id(resource_id);
  tile->setOpaqueRect(opaque_rect);
  tile->set_contents_swizzled(contents_swizzled);
}

void TiledLayerImpl::PushInvalidTile(int i, int j) {
  DrawableTile* tile = TileAt(i, j);
  if (!tile)
    tile = CreateTile(i, j);
  tile->set_resource_id(0);
  tile->setOpaqueRect(gfx::Rect());
  tile->set_contents_swizzled(false);
}

Region TiledLayerImpl::VisibleContentOpaqueRegion() const {
  if (skips_draw_)
    return Region();
  if (contents_opaque())
    return visible_content_rect();
  return tiler_->opaqueRegionInContentRect(visible_content_rect());
}

void TiledLayerImpl::DidLoseOutputSurface() {
  s_safe_to_delete_drawable_tile = true;
  // Temporary diagnostic check.
  CHECK(tiler_);
  tiler_->reset();
  s_safe_to_delete_drawable_tile = false;
}

const char* TiledLayerImpl::LayerTypeAsString() const {
  return "ContentLayer";
}

}  // namespace cc
