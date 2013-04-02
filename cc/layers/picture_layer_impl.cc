// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include <algorithm>

#include "base/time.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace {
const float kMaxScaleRatioDuringPinch = 2.0f;
}

namespace cc {

PictureLayerImpl::PictureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      pile_(PicturePileImpl::Create(true)),
      last_content_scale_(0),
      is_mask_(false),
      ideal_page_scale_(0.f),
      ideal_device_scale_(0.f),
      ideal_source_scale_(0.f),
      ideal_contents_scale_(0.f),
      raster_page_scale_(0.f),
      raster_device_scale_(0.f),
      raster_source_scale_(0.f),
      raster_contents_scale_(0.f),
      low_res_raster_contents_scale_(0.f),
      raster_source_scale_was_animating_(false),
      is_using_lcd_text_(true) {
}

PictureLayerImpl::~PictureLayerImpl() {
}

const char* PictureLayerImpl::LayerTypeAsString() const {
  return "PictureLayer";
}

scoped_ptr<LayerImpl> PictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void PictureLayerImpl::CreateTilingSet() {
  DCHECK(layer_tree_impl()->IsPendingTree());
  DCHECK(!tilings_);
  tilings_.reset(new PictureLayerTilingSet(this));
  tilings_->SetLayerBounds(bounds());
}

void PictureLayerImpl::TransferTilingSet(
    scoped_ptr<PictureLayerTilingSet> tilings) {
  DCHECK(layer_tree_impl()->IsActiveTree());
  tilings->SetClient(this);
  tilings_ = tilings.Pass();
}

void PictureLayerImpl::PushPropertiesTo(LayerImpl* base_layer) {
  LayerImpl::PushPropertiesTo(base_layer);

  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  layer_impl->SetIsMask(is_mask_);
  layer_impl->TransferTilingSet(tilings_.Pass());
  layer_impl->pile_ = pile_;
  pile_ = PicturePileImpl::Create(is_using_lcd_text_);

  layer_impl->raster_page_scale_ = raster_page_scale_;
  layer_impl->raster_device_scale_ = raster_device_scale_;
  layer_impl->raster_source_scale_ = raster_source_scale_;
  layer_impl->raster_contents_scale_ = raster_contents_scale_;
  layer_impl->low_res_raster_contents_scale_ = low_res_raster_contents_scale_;
  layer_impl->is_using_lcd_text_ = is_using_lcd_text_;
}


void PictureLayerImpl::AppendQuads(QuadSink* quad_sink,
                                   AppendQuadsData* append_quads_data) {
  const gfx::Rect& rect = visible_content_rect();
  gfx::Rect content_rect(content_bounds());

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  bool clipped = false;
  gfx::QuadF target_quad = MathUtil::MapQuad(
      draw_transform(),
      gfx::QuadF(rect),
      &clipped);
  if (ShowDebugBorders()) {
    for (PictureLayerTilingSet::CoverageIterator iter(
        tilings_.get(), contents_scale_x(), rect, ideal_contents_scale_);
         iter;
         ++iter) {
      SkColor color;
      float width;
      if (*iter && iter->drawing_info().IsReadyToDraw()) {
        ManagedTileState::DrawingInfo::Mode mode = iter->drawing_info().mode();
        if (mode == ManagedTileState::DrawingInfo::SOLID_COLOR_MODE ||
            mode == ManagedTileState::DrawingInfo::TRANSPARENT_MODE) {
          color = DebugColors::SolidColorTileBorderColor();
          width = DebugColors::SolidColorTileBorderWidth(layer_tree_impl());
        } else if (mode == ManagedTileState::DrawingInfo::PICTURE_PILE_MODE) {
          color = DebugColors::PictureTileBorderColor();
          width = DebugColors::PictureTileBorderWidth(layer_tree_impl());
        } else if (iter->priority(ACTIVE_TREE).resolution == HIGH_RESOLUTION) {
          color = DebugColors::HighResTileBorderColor();
          width = DebugColors::HighResTileBorderWidth(layer_tree_impl());
        } else if (iter->priority(ACTIVE_TREE).resolution == LOW_RESOLUTION) {
          color = DebugColors::LowResTileBorderColor();
          width = DebugColors::LowResTileBorderWidth(layer_tree_impl());
        } else if (iter->contents_scale() > contents_scale_x()) {
          color = DebugColors::ExtraHighResTileBorderColor();
          width = DebugColors::ExtraHighResTileBorderWidth(layer_tree_impl());
        } else {
          color = DebugColors::ExtraLowResTileBorderColor();
          width = DebugColors::ExtraLowResTileBorderWidth(layer_tree_impl());
        }
      } else {
        color = DebugColors::MissingTileBorderColor();
        width = DebugColors::MissingTileBorderWidth(layer_tree_impl());
      }

      scoped_ptr<DebugBorderDrawQuad> debug_border_quad =
          DebugBorderDrawQuad::Create();
      gfx::Rect geometry_rect = iter.geometry_rect();
      debug_border_quad->SetNew(shared_quad_state, geometry_rect, color, width);
      quad_sink->Append(debug_border_quad.PassAs<DrawQuad>(),
                        append_quads_data);
    }
  }

  // Keep track of the tilings that were used so that tilings that are
  // unused can be considered for removal.
  std::vector<PictureLayerTiling*> seen_tilings;

  for (PictureLayerTilingSet::CoverageIterator iter(
      tilings_.get(), contents_scale_x(), rect, ideal_contents_scale_);
       iter;
       ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    if (!*iter || !iter->drawing_info().IsReadyToDraw()) {
      if (DrawCheckerboardForMissingTiles()) {
        // TODO(enne): Figure out how to show debug "invalidated checker" color
        scoped_ptr<CheckerboardDrawQuad> quad = CheckerboardDrawQuad::Create();
        SkColor color = DebugColors::DefaultCheckerboardColor();
        quad->SetNew(shared_quad_state, geometry_rect, color);
        if (quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data))
          append_quads_data->num_missing_tiles++;
      } else {
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(shared_quad_state, geometry_rect, background_color());
        if (quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data))
          append_quads_data->num_missing_tiles++;
      }

      append_quads_data->had_incomplete_tile = true;
      continue;
    }

    const ManagedTileState::DrawingInfo& drawing_info = iter->drawing_info();
    switch (drawing_info.mode()) {
      case ManagedTileState::DrawingInfo::RESOURCE_MODE: {
        gfx::RectF texture_rect = iter.texture_rect();
        gfx::Rect opaque_rect = iter->opaque_rect();
        opaque_rect.Intersect(content_rect);

        if (iter->contents_scale() != ideal_contents_scale_)
          append_quads_data->had_incomplete_tile = true;

        scoped_ptr<TileDrawQuad> quad = TileDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     opaque_rect,
                     drawing_info.get_resource_id(),
                     texture_rect,
                     iter.texture_size(),
                     drawing_info.contents_swizzled());
        quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
        break;
      }
      case ManagedTileState::DrawingInfo::PICTURE_PILE_MODE: {
        gfx::RectF texture_rect = iter.texture_rect();
        gfx::Rect opaque_rect = iter->opaque_rect();
        opaque_rect.Intersect(content_rect);

        scoped_ptr<PictureDrawQuad> quad = PictureDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     opaque_rect,
                     texture_rect,
                     iter.texture_size(),
                     drawing_info.contents_swizzled(),
                     iter->content_rect(),
                     iter->contents_scale(),
                     pile_);
        quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
        break;
      }
      case ManagedTileState::DrawingInfo::SOLID_COLOR_MODE: {
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     drawing_info.get_solid_color());
        quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
        break;
      }
      case ManagedTileState::DrawingInfo::TRANSPARENT_MODE:
        break;
      default:
        NOTREACHED();
    }

    if (!seen_tilings.size() || seen_tilings.back() != iter.CurrentTiling())
      seen_tilings.push_back(iter.CurrentTiling());
  }

  // Aggressively remove any tilings that are not seen to save memory. Note
  // that this is at the expense of doing cause more frequent re-painting. A
  // better scheme would be to maintain a tighter visible_content_rect for the
  // finer tilings.
  CleanUpTilingsOnActiveLayer(seen_tilings);
}

void PictureLayerImpl::DumpLayerProperties(std::string*, int indent) const {
  // TODO(enne): implement me
}

void PictureLayerImpl::UpdateTilePriorities() {
  UpdateLCDTextStatus();

  int current_source_frame_number = layer_tree_impl()->source_frame_number();
  double current_frame_time =
      (layer_tree_impl()->CurrentFrameTime() - base::TimeTicks()).InSecondsF();

  gfx::Transform current_screen_space_transform = screen_space_transform();

  gfx::Rect viewport_in_content_space;
  gfx::Transform screen_to_layer(gfx::Transform::kSkipInitialization);
  if (screen_space_transform().GetInverse(&screen_to_layer)) {
    gfx::Rect device_viewport(layer_tree_impl()->device_viewport_size());
    viewport_in_content_space = gfx::ToEnclosingRect(
        MathUtil::ProjectClippedRect(screen_to_layer, device_viewport));
  }

  WhichTree tree =
      layer_tree_impl()->IsActiveTree() ? ACTIVE_TREE : PENDING_TREE;
  bool store_screen_space_quads_on_tiles =
      layer_tree_impl()->debug_state().trace_all_rendered_frames;
  size_t max_tiles_for_interest_area =
      layer_tree_impl()->settings().max_tiles_for_interest_area;
  tilings_->UpdateTilePriorities(
      tree,
      layer_tree_impl()->device_viewport_size(),
      viewport_in_content_space,
      last_bounds_,
      bounds(),
      last_content_scale_,
      contents_scale_x(),
      last_screen_space_transform_,
      current_screen_space_transform,
      current_source_frame_number,
      current_frame_time,
      store_screen_space_quads_on_tiles,
      max_tiles_for_interest_area);

  last_screen_space_transform_ = current_screen_space_transform;
  last_bounds_ = bounds();
  last_content_scale_ = contents_scale_x();
}

void PictureLayerImpl::DidBecomeActive() {
  LayerImpl::DidBecomeActive();
  tilings_->DidBecomeActive();
}

void PictureLayerImpl::DidLoseOutputSurface() {
  if (tilings_)
    tilings_->RemoveAllTilings();

  raster_page_scale_ = 0;
  raster_device_scale_ = 0;
  raster_source_scale_ = 0;
}

void PictureLayerImpl::CalculateContentsScale(
    float ideal_contents_scale,
    bool animating_transform_to_screen,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  if (!DrawsContent()) {
    DCHECK(!tilings_->num_tilings());
    return;
  }

  float min_contents_scale = MinimumContentsScale();
  float min_page_scale = layer_tree_impl()->min_page_scale_factor();
  float min_device_scale = 1.f;
  float min_source_scale =
      min_contents_scale / min_page_scale / min_device_scale;

  float ideal_page_scale = layer_tree_impl()->total_page_scale_factor();
  float ideal_device_scale = layer_tree_impl()->device_scale_factor();
  float ideal_source_scale =
      ideal_contents_scale / ideal_page_scale / ideal_device_scale;

  ideal_contents_scale_ = std::max(ideal_contents_scale, min_contents_scale);
  ideal_page_scale_ = ideal_page_scale;
  ideal_device_scale_ = ideal_device_scale;
  ideal_source_scale_ = std::max(ideal_source_scale, min_source_scale);

  ManageTilings(animating_transform_to_screen);

  // The content scale and bounds for a PictureLayerImpl is somewhat fictitious.
  // There are (usually) several tilings at different scales.  However, the
  // content bounds is the (integer!) space in which quads are generated.
  // In order to guarantee that we can fill this integer space with any set of
  // tilings (and then map back to floating point texture coordinates), the
  // contents scale must be at least as large as the largest of the tilings.
  float max_contents_scale = min_contents_scale;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings_->tiling_at(i);
    max_contents_scale = std::max(max_contents_scale, tiling->contents_scale());
  }

  *contents_scale_x = max_contents_scale;
  *contents_scale_y = max_contents_scale;
  *content_bounds = gfx::ToCeiledSize(
      gfx::ScaleSize(bounds(), max_contents_scale, max_contents_scale));
}

skia::RefPtr<SkPicture> PictureLayerImpl::GetPicture() {
  return pile_->GetFlattenedPicture();
}

scoped_refptr<Tile> PictureLayerImpl::CreateTile(PictureLayerTiling* tiling,
                                                 gfx::Rect content_rect) {
  if (!pile_->CanRaster(tiling->contents_scale(), content_rect))
    return scoped_refptr<Tile>();

  return make_scoped_refptr(new Tile(
      layer_tree_impl()->tile_manager(),
      pile_.get(),
      content_rect.size(),
      GL_RGBA,
      content_rect,
      contents_opaque() ? content_rect : gfx::Rect(),
      tiling->contents_scale(),
      id()));
}

void PictureLayerImpl::UpdatePile(Tile* tile) {
  tile->set_picture_pile(pile_);
}

gfx::Size PictureLayerImpl::CalculateTileSize(
    gfx::Size current_tile_size,
    gfx::Size content_bounds) {
  if (is_mask_) {
    int max_size = layer_tree_impl()->MaxTextureSize();
    return gfx::Size(
        std::min(max_size, content_bounds.width()),
        std::min(max_size, content_bounds.height()));
  }

  gfx::Size default_tile_size = layer_tree_impl()->settings().default_tile_size;
  gfx::Size max_untiled_content_size =
      layer_tree_impl()->settings().max_untiled_layer_size;

  bool any_dimension_too_large =
      content_bounds.width() > max_untiled_content_size.width() ||
      content_bounds.height() > max_untiled_content_size.height();

  bool any_dimension_one_tile =
      content_bounds.width() <= default_tile_size.width() ||
      content_bounds.height() <= default_tile_size.height();

  // If long and skinny, tile at the max untiled content size, and clamp
  // the smaller dimension to the content size, e.g. 1000x12 layer with
  // 500x500 max untiled size would get 500x12 tiles.  Also do this
  // if the layer is small.
  if (any_dimension_one_tile || !any_dimension_too_large) {
    int width =
        std::min(max_untiled_content_size.width(), content_bounds.width());
    int height =
        std::min(max_untiled_content_size.height(), content_bounds.height());
    // Round width and height up to the closest multiple of 64, or 56 if
    // we should avoid power-of-two textures. This helps reduce the number
    // of different textures sizes to help recycling, and also keeps all
    // textures multiple-of-eight, which is preferred on some drivers (IMG).
    bool avoid_pow2 =
        layer_tree_impl()->GetRendererCapabilities().avoid_pow2_textures;
    int round_up_to = avoid_pow2 ? 56 : 64;
    width = RoundUp(width, round_up_to);
    height = RoundUp(height, round_up_to);
    return gfx::Size(width, height);
  }

  return default_tile_size;
}

void PictureLayerImpl::SyncFromActiveLayer() {
  DCHECK(layer_tree_impl()->IsPendingTree());

  if (!DrawsContent()) {
    raster_page_scale_ = 0;
    raster_device_scale_ = 0;
    raster_source_scale_ = 0;
    raster_contents_scale_ = 0;
    low_res_raster_contents_scale_ = 0;
    return;
  }

  // If there is an active tree version of this layer, get a copy of its
  // tiles.  This needs to be done last, after setting invalidation and the
  // pile.
  if (PictureLayerImpl* active_twin = ActiveTwin())
    SyncFromActiveLayer(active_twin);
}

void PictureLayerImpl::SyncFromActiveLayer(const PictureLayerImpl* other) {
  raster_page_scale_ = other->raster_page_scale_;
  raster_device_scale_ = other->raster_device_scale_;
  raster_source_scale_ = other->raster_source_scale_;
  raster_contents_scale_ = other->raster_contents_scale_;
  low_res_raster_contents_scale_ = other->low_res_raster_contents_scale_;
  is_using_lcd_text_ = other->is_using_lcd_text_;

  // Add synthetic invalidations for any recordings that were dropped.  As
  // tiles are updated to point to this new pile, this will force the dropping
  // of tiles that can no longer be rastered.  This is not ideal, but is a
  // trade-off for memory (use the same pile as much as possible, by switching
  // during DidBecomeActive) and for time (don't bother checking every tile
  // during activation to see if the new pile can still raster it).
  //
  // TODO(enne): Clean up this double loop.
  for (int x = 0; x < pile_->num_tiles_x(); ++x) {
    for (int y = 0; y < pile_->num_tiles_y(); ++y) {
      bool previously_had = other->pile_->HasRecordingAt(x, y);
      bool now_has = pile_->HasRecordingAt(x, y);
      if (now_has || !previously_had)
        continue;
      gfx::Rect layer_rect = pile_->tile_bounds(x, y);
      invalidation_.Union(layer_rect);
    }
  }

  tilings_->CloneAll(*other->tilings_, invalidation_, MinimumContentsScale());
  DCHECK(bounds() == tilings_->LayerBounds());

  // It's a sad but unfortunate fact that PicturePile tiling edges do not line
  // up with PictureLayerTiling edges.  Tiles can only be added if they are
  // entirely covered by recordings (that may come from multiple PicturePile
  // tiles).  This check happens in this class's CreateTile() call.
  for (int x = 0; x < pile_->num_tiles_x(); ++x) {
    for (int y = 0; y < pile_->num_tiles_y(); ++y) {
      bool previously_had = other->pile_->HasRecordingAt(x, y);
      bool now_has = pile_->HasRecordingAt(x, y);
      if (!now_has || previously_had)
        continue;
      gfx::Rect layer_rect = pile_->tile_bounds(x, y);
      tilings_->CreateTilesFromLayerRect(layer_rect);
    }
  }
}

void PictureLayerImpl::SyncTiling(
    const PictureLayerTiling* tiling,
    const Region& pending_layer_invalidation) {
  if (!DrawsContent() || tiling->contents_scale() < MinimumContentsScale())
    return;
  tilings_->Clone(tiling, pending_layer_invalidation);
}

void PictureLayerImpl::SetIsMask(bool is_mask) {
  if (is_mask_ == is_mask)
    return;
  is_mask_ = is_mask;
  if (tilings_)
    tilings_->RemoveAllTiles();
}

ResourceProvider::ResourceId PictureLayerImpl::ContentsResourceId() const {
  gfx::Rect content_rect(content_bounds());
  float scale = contents_scale_x();
  for (PictureLayerTilingSet::CoverageIterator
           iter(tilings_.get(), scale, content_rect, ideal_contents_scale_);
       iter;
       ++iter) {
    // Mask resource not ready yet.
    if (!*iter ||
        iter->drawing_info().mode() !=
            ManagedTileState::DrawingInfo::RESOURCE_MODE ||
        !iter->drawing_info().IsReadyToDraw())
      return 0;
    // Masks only supported if they fit on exactly one tile.
    if (iter.geometry_rect() != content_rect)
      return 0;
    return iter->drawing_info().get_resource_id();
  }
  return 0;
}

bool PictureLayerImpl::AreVisibleResourcesReady() const {
  DCHECK(layer_tree_impl()->IsPendingTree());
  DCHECK(ideal_contents_scale_);

  const gfx::Rect& rect = visible_content_rect();

  float min_acceptable_scale =
      std::min(raster_contents_scale_, ideal_contents_scale_);

  TreePriority tree_priority =
      layer_tree_impl()->tile_manager()->GlobalState().tree_priority;
  bool should_force_uploads =
      tree_priority != SMOOTHNESS_TAKES_PRIORITY &&
      layer_tree_impl()->animationRegistrar()->
          active_animation_controllers().empty();

  if (PictureLayerImpl* twin = ActiveTwin()) {
    float twin_min_acceptable_scale =
        std::min(twin->ideal_contents_scale_, twin->raster_contents_scale_);
    // Ignore 0 scale in case CalculateContentsScale() has never been
    // called for active twin.
    if (twin_min_acceptable_scale != 0.0f) {
      min_acceptable_scale =
          std::min(min_acceptable_scale, twin_min_acceptable_scale);
    }
  }

  Region missing_region = rect;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);

    if (tiling->contents_scale() < min_acceptable_scale)
      continue;

    for (PictureLayerTiling::CoverageIterator iter(tiling,
                                                   contents_scale_x(),
                                                   rect);
         iter;
         ++iter) {
      if (should_force_uploads && *iter)
        layer_tree_impl()->tile_manager()->ForceTileUploadToComplete(*iter);
      // A null tile (i.e. no recording) is considered "ready".
      if (!*iter || iter->drawing_info().IsReadyToDraw())
        missing_region.Subtract(iter.geometry_rect());
    }
  }

  return missing_region.IsEmpty();
}

PictureLayerTiling* PictureLayerImpl::AddTiling(float contents_scale) {
  DCHECK(contents_scale >= MinimumContentsScale());

  PictureLayerTiling* tiling = tilings_->AddTiling(contents_scale);

  const Region& recorded = pile_->recorded_region();
  DCHECK(!recorded.IsEmpty());

  for (Region::Iterator iter(recorded); iter.has_rect(); iter.next())
    tiling->CreateTilesFromLayerRect(iter.rect());

  PictureLayerImpl* twin =
      layer_tree_impl()->IsPendingTree() ? ActiveTwin() : PendingTwin();
  if (!twin)
    return tiling;

  if (layer_tree_impl()->IsPendingTree())
    twin->SyncTiling(tiling, invalidation_);
  else
    twin->SyncTiling(tiling, twin->invalidation_);

  return tiling;
}

void PictureLayerImpl::RemoveTiling(float contents_scale) {
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (tiling->contents_scale() == contents_scale) {
      tilings_->Remove(tiling);
      break;
    }
  }
}

namespace {

inline float PositiveRatio(float float1, float float2) {
  DCHECK_GT(float1, 0);
  DCHECK_GT(float2, 0);
  return float1 > float2 ? float1 / float2 : float2 / float1;
}

inline bool IsCloserToThan(
    PictureLayerTiling* layer1,
    PictureLayerTiling* layer2,
    float contents_scale) {
  // Absolute value for ratios.
  float ratio1 = PositiveRatio(layer1->contents_scale(), contents_scale);
  float ratio2 = PositiveRatio(layer2->contents_scale(), contents_scale);
  return ratio1 < ratio2;
}

}  // namespace

void PictureLayerImpl::ManageTilings(bool animating_transform_to_screen) {
  DCHECK(ideal_contents_scale_);
  DCHECK(ideal_page_scale_);
  DCHECK(ideal_device_scale_);
  DCHECK(ideal_source_scale_);

  if (pile_->recorded_region().IsEmpty())
    return;

  bool change_target_tiling =
      !raster_page_scale_ || !raster_device_scale_ || !raster_source_scale_ ||
      ShouldAdjustRasterScale(animating_transform_to_screen);

  if (layer_tree_impl()->IsActiveTree()) {
    // Store the value for the next time ShouldAdjustRasterScale is called.
    raster_source_scale_was_animating_ = animating_transform_to_screen;
  }

  if (!change_target_tiling)
    return;

  raster_page_scale_ = ideal_page_scale_;
  raster_device_scale_ = ideal_device_scale_;
  raster_source_scale_ = ideal_source_scale_;

  CalculateRasterContentsScale(animating_transform_to_screen,
                               &raster_contents_scale_,
                               &low_res_raster_contents_scale_);

  PictureLayerTiling* high_res = NULL;
  PictureLayerTiling* low_res = NULL;

  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (tiling->contents_scale() == raster_contents_scale_)
      high_res = tiling;
    if (tiling->contents_scale() == low_res_raster_contents_scale_)
      low_res = tiling;

    // Reset all tilings to non-ideal until the end of this function.
    tiling->set_resolution(NON_IDEAL_RESOLUTION);
  }

  if (!high_res) {
    high_res = AddTiling(raster_contents_scale_);
    if (raster_contents_scale_ == low_res_raster_contents_scale_)
      low_res = high_res;
  }
  if (!low_res && low_res != high_res)
    low_res = AddTiling(low_res_raster_contents_scale_);

  if (high_res)
    high_res->set_resolution(HIGH_RESOLUTION);
  if (low_res && low_res != high_res)
    low_res->set_resolution(LOW_RESOLUTION);
}

bool PictureLayerImpl::ShouldAdjustRasterScale(
    bool animating_transform_to_screen) const {
  // TODO(danakj): Adjust raster source scale closer to ideal source scale at
  // a throttled rate. Possibly make use of invalidation_.IsEmpty() on pending
  // tree. This will allow CSS scale changes to get re-rastered at an
  // appropriate rate.

  bool is_active_layer = layer_tree_impl()->IsActiveTree();
  if (is_active_layer && raster_source_scale_was_animating_ &&
      !animating_transform_to_screen)
    return true;

  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (is_active_layer && is_pinching && raster_page_scale_) {
    // If the page scale diverges too far during pinch, change raster target to
    // the current page scale.
    float ratio = PositiveRatio(ideal_page_scale_, raster_page_scale_);
    if (ratio >= kMaxScaleRatioDuringPinch)
      return true;
  }

  if (!is_pinching) {
    // When not pinching, match the ideal page scale factor.
    if (raster_page_scale_ != ideal_page_scale_)
      return true;
  }

  // Always match the ideal device scale factor.
  if (raster_device_scale_ != ideal_device_scale_)
    return true;

  return false;
}

void PictureLayerImpl::CalculateRasterContentsScale(
    bool animating_transform_to_screen,
    float* raster_contents_scale,
    float* low_res_raster_contents_scale) const {
  *raster_contents_scale = ideal_contents_scale_;

  // Don't allow animating CSS scales to drop below 1.
  if (animating_transform_to_screen) {
    *raster_contents_scale = std::max(
        *raster_contents_scale, 1.f * ideal_page_scale_ * ideal_device_scale_);
  }

  float low_res_factor =
      layer_tree_impl()->settings().low_res_contents_scale_factor;
  *low_res_raster_contents_scale = std::max(
      *raster_contents_scale * low_res_factor,
      MinimumContentsScale());
}

void PictureLayerImpl::CleanUpTilingsOnActiveLayer(
    std::vector<PictureLayerTiling*> used_tilings) {
  DCHECK(layer_tree_impl()->IsActiveTree());

  float min_acceptable_high_res_scale = std::min(
      raster_contents_scale_, ideal_contents_scale_);
  float max_acceptable_high_res_scale = std::max(
      raster_contents_scale_, ideal_contents_scale_);

  PictureLayerImpl* twin = PendingTwin();
  if (twin) {
    min_acceptable_high_res_scale = std::min(
        min_acceptable_high_res_scale,
        std::min(twin->raster_contents_scale_, twin->ideal_contents_scale_));
    max_acceptable_high_res_scale = std::max(
        max_acceptable_high_res_scale,
        std::max(twin->raster_contents_scale_, twin->ideal_contents_scale_));
  }

  float low_res_factor =
      layer_tree_impl()->settings().low_res_contents_scale_factor;

  float min_acceptable_low_res_scale =
      low_res_factor * min_acceptable_high_res_scale;
  float max_acceptable_low_res_scale =
      low_res_factor * max_acceptable_high_res_scale;

  std::vector<PictureLayerTiling*> to_remove;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);

    if (tiling->contents_scale() >= min_acceptable_high_res_scale &&
        tiling->contents_scale() <= max_acceptable_high_res_scale)
      continue;

    if (tiling->contents_scale() >= min_acceptable_low_res_scale &&
        tiling->contents_scale() <= max_acceptable_low_res_scale)
      continue;

    // Don't remove tilings that are being used and expected to stay around.
    if (std::find(used_tilings.begin(), used_tilings.end(), tiling) !=
        used_tilings.end())
      continue;

    to_remove.push_back(tiling);
  }

  for (size_t i = 0; i < to_remove.size(); ++i) {
    if (twin)
      twin->RemoveTiling(to_remove[i]->contents_scale());
    tilings_->Remove(to_remove[i]);
  }
}

PictureLayerImpl* PictureLayerImpl::PendingTwin() const {
  DCHECK(layer_tree_impl()->IsActiveTree());

  PictureLayerImpl* twin = static_cast<PictureLayerImpl*>(
      layer_tree_impl()->FindPendingTreeLayerById(id()));
  if (twin)
    DCHECK_EQ(id(), twin->id());
  return twin;
}

PictureLayerImpl* PictureLayerImpl::ActiveTwin() const {
  DCHECK(layer_tree_impl()->IsPendingTree());

  PictureLayerImpl* twin = static_cast<PictureLayerImpl*>(
      layer_tree_impl()->FindActiveTreeLayerById(id()));
  if (twin)
    DCHECK_EQ(id(), twin->id());
  return twin;
}

float PictureLayerImpl::MinimumContentsScale() const {
  float setting_min = layer_tree_impl()->settings().minimum_contents_scale;

  // If the contents scale is less than 1 / width (also for height),
  // then it will end up having less than one pixel of content in that
  // dimension.  Bump the minimum contents scale up in this case to prevent
  // this from happening.
  int min_dimension = std::min(bounds().width(), bounds().height());
  if (!min_dimension)
    return setting_min;

  return std::max(1.f / min_dimension, setting_min);
}

void PictureLayerImpl::UpdateLCDTextStatus() {
  // Once this layer is not using lcd text, don't switch back.
  if (!is_using_lcd_text_)
    return;

  if (is_using_lcd_text_ == can_use_lcd_text())
    return;

  is_using_lcd_text_ = can_use_lcd_text();

  // As a trade-off between jank and drawing with the incorrect resources,
  // don't ever update the active tree's resources in place.  Instead,
  // update lcd text on the pending tree.  If this is the active tree and
  // there is no pending twin, then call set needs commit to create one.
  if (layer_tree_impl()->IsActiveTree() && !PendingTwin()) {
    // TODO(enne): Handle this by updating these resources in-place instead.
    layer_tree_impl()->SetNeedsCommit();
    return;
  }

  // The heuristic of never switching back to lcd text enabled implies that
  // this property needs to be synchronized to the pending tree right now.
  PictureLayerImpl* pending_layer =
      layer_tree_impl()->IsActiveTree() ? PendingTwin() : this;
  if (layer_tree_impl()->IsActiveTree() &&
      pending_layer->is_using_lcd_text_ == is_using_lcd_text_)
    return;

  // Further tiles created due to new tilings should be considered invalidated.
  pending_layer->invalidation_.Union(gfx::Rect(bounds()));
  pending_layer->is_using_lcd_text_ = is_using_lcd_text_;
  pending_layer->pile_ = PicturePileImpl::CreateFromOther(pending_layer->pile_,
                                                          is_using_lcd_text_);

  // TODO(enne): if we tracked text regions, we could just invalidate those
  // directly rather than tossing away every tile.
  pending_layer->tilings_->Invalidate(gfx::Rect(bounds()));
}

void PictureLayerImpl::GetDebugBorderProperties(
    SkColor* color,
    float* width) const {
  *color = DebugColors::TiledContentLayerBorderColor();
  *width = DebugColors::TiledContentLayerBorderWidth(layer_tree_impl());
}

scoped_ptr<base::Value> PictureLayerImpl::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  LayerImpl::AsValueInto(state.get());

  state->SetDouble("ideal_contents_scale", ideal_contents_scale_);
  state->Set("tilings", tilings_->AsValue().release());
  return state.PassAs<base::Value>();
}

}  // namespace cc
