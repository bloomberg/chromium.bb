// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include <algorithm>
#include <limits>

#include "base/time/time.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/micro_benchmark_impl.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/tile_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace {
const float kMaxScaleRatioDuringPinch = 2.0f;

// When creating a new tiling during pinch, snap to an existing
// tiling's scale if the desired scale is within this ratio.
const float kSnapToExistingTilingRatio = 0.2f;
}

namespace cc {

PictureLayerImpl::PictureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      twin_layer_(NULL),
      pile_(PicturePileImpl::Create()),
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
      is_using_lcd_text_(tree_impl->settings().can_use_lcd_text),
      needs_post_commit_initialization_(true),
      should_update_tile_priorities_(false),
      should_use_low_res_tiling_(tree_impl->settings().create_low_res_tiling),
      use_gpu_rasterization_(false),
      layer_needs_to_register_itself_(true) {}

PictureLayerImpl::~PictureLayerImpl() {
  if (!layer_needs_to_register_itself_)
    layer_tree_impl()->tile_manager()->UnregisterPictureLayerImpl(this);
}

const char* PictureLayerImpl::LayerTypeAsString() const {
  return "cc::PictureLayerImpl";
}

scoped_ptr<LayerImpl> PictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void PictureLayerImpl::PushPropertiesTo(LayerImpl* base_layer) {
  // It's possible this layer was never drawn or updated (e.g. because it was
  // a descendant of an opacity 0 layer).
  DoPostCommitInitializationIfNeeded();
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  // We have already synced the important bits from the the active layer, and
  // we will soon swap out its tilings and use them for recycling. However,
  // there are now tiles in this layer's tilings that were unref'd and replaced
  // with new tiles (due to invalidation). This resets all active priorities on
  // the to-be-recycled tiling to ensure replaced tiles don't linger and take
  // memory (due to a stale 'active' priority).
  if (layer_impl->tilings_)
    layer_impl->tilings_->DidBecomeRecycled();

  LayerImpl::PushPropertiesTo(base_layer);

  // When the pending tree pushes to the active tree, the pending twin
  // disappears.
  layer_impl->twin_layer_ = NULL;
  twin_layer_ = NULL;

  layer_impl->SetIsMask(is_mask_);
  layer_impl->pile_ = pile_;
  layer_impl->use_gpu_rasterization_ = use_gpu_rasterization_;

  // Tilings would be expensive to push, so we swap.
  layer_impl->tilings_.swap(tilings_);

  // Ensure that we don't have any tiles that are out of date.
  if (tilings_)
    tilings_->RemoveTilesInRegion(invalidation_);

  layer_impl->tilings_->SetClient(layer_impl);
  if (tilings_)
    tilings_->SetClient(this);

  layer_impl->raster_page_scale_ = raster_page_scale_;
  layer_impl->raster_device_scale_ = raster_device_scale_;
  layer_impl->raster_source_scale_ = raster_source_scale_;
  layer_impl->raster_contents_scale_ = raster_contents_scale_;
  layer_impl->low_res_raster_contents_scale_ = low_res_raster_contents_scale_;

  layer_impl->UpdateLCDTextStatus(is_using_lcd_text_);
  layer_impl->needs_post_commit_initialization_ = false;

  // The invalidation on this soon-to-be-recycled layer must be cleared to
  // mirror clearing the invalidation in PictureLayer's version of this function
  // in case push properties is skipped.
  layer_impl->invalidation_.Swap(&invalidation_);
  invalidation_.Clear();
  needs_post_commit_initialization_ = true;

  // We always need to push properties.
  // See http://crbug.com/303943
  needs_push_properties_ = true;
}

void PictureLayerImpl::AppendQuads(QuadSink* quad_sink,
                                   AppendQuadsData* append_quads_data) {
  DCHECK(!needs_post_commit_initialization_);
  gfx::Rect rect(visible_content_rect());
  gfx::Rect content_rect(content_bounds());

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());

  if (current_draw_mode_ == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    AppendDebugBorderQuad(
        quad_sink,
        shared_quad_state,
        append_quads_data,
        DebugColors::DirectPictureBorderColor(),
        DebugColors::DirectPictureBorderWidth(layer_tree_impl()));

    gfx::Rect geometry_rect = rect;
    gfx::Rect opaque_rect = contents_opaque() ? geometry_rect : gfx::Rect();
    gfx::Rect visible_geometry_rect = geometry_rect;
    gfx::Size texture_size = rect.size();
    gfx::RectF texture_rect = gfx::RectF(texture_size);
    gfx::Rect quad_content_rect = rect;
    float contents_scale = contents_scale_x();

    scoped_ptr<PictureDrawQuad> quad = PictureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 geometry_rect,
                 opaque_rect,
                 visible_geometry_rect,
                 texture_rect,
                 texture_size,
                 RGBA_8888,
                 quad_content_rect,
                 contents_scale,
                 pile_);
    if (quad_sink->MaybeAppend(quad.PassAs<DrawQuad>()))
      append_quads_data->num_missing_tiles++;
    return;
  }

  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  if (ShowDebugBorders()) {
    for (PictureLayerTilingSet::CoverageIterator iter(
        tilings_.get(), contents_scale_x(), rect, ideal_contents_scale_);
         iter;
         ++iter) {
      SkColor color;
      float width;
      if (*iter && iter->IsReadyToDraw()) {
        ManagedTileState::TileVersion::Mode mode =
            iter->GetTileVersionForDrawing().mode();
        if (mode == ManagedTileState::TileVersion::SOLID_COLOR_MODE) {
          color = DebugColors::SolidColorTileBorderColor();
          width = DebugColors::SolidColorTileBorderWidth(layer_tree_impl());
        } else if (mode == ManagedTileState::TileVersion::PICTURE_PILE_MODE) {
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
      gfx::Rect visible_geometry_rect = geometry_rect;
      debug_border_quad->SetNew(shared_quad_state,
                                geometry_rect,
                                visible_geometry_rect,
                                color,
                                width);
      quad_sink->MaybeAppend(debug_border_quad.PassAs<DrawQuad>());
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
    gfx::Rect visible_geometry_rect = geometry_rect;
    if (!*iter || !iter->IsReadyToDraw()) {
      if (draw_checkerboard_for_missing_tiles()) {
        // TODO(enne): Figure out how to show debug "invalidated checker" color
        scoped_ptr<CheckerboardDrawQuad> quad = CheckerboardDrawQuad::Create();
        SkColor color = DebugColors::DefaultCheckerboardColor();
        quad->SetNew(
            shared_quad_state, geometry_rect, visible_geometry_rect, color);
        if (quad_sink->MaybeAppend(quad.PassAs<DrawQuad>()))
          append_quads_data->num_missing_tiles++;
      } else {
        SkColor color = SafeOpaqueBackgroundColor();
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     visible_geometry_rect,
                     color,
                     false);
        if (quad_sink->MaybeAppend(quad.PassAs<DrawQuad>()))
          append_quads_data->num_missing_tiles++;
      }

      append_quads_data->had_incomplete_tile = true;
      continue;
    }

    const ManagedTileState::TileVersion& tile_version =
        iter->GetTileVersionForDrawing();
    scoped_ptr<DrawQuad> draw_quad;
    switch (tile_version.mode()) {
      case ManagedTileState::TileVersion::RESOURCE_MODE: {
        gfx::RectF texture_rect = iter.texture_rect();
        gfx::Rect opaque_rect = iter->opaque_rect();
        opaque_rect.Intersect(geometry_rect);

        if (iter->contents_scale() != ideal_contents_scale_)
          append_quads_data->had_incomplete_tile = true;

        scoped_ptr<TileDrawQuad> quad = TileDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     opaque_rect,
                     visible_geometry_rect,
                     tile_version.get_resource_id(),
                     texture_rect,
                     iter.texture_size(),
                     tile_version.contents_swizzled());
        draw_quad = quad.PassAs<DrawQuad>();
        break;
      }
      case ManagedTileState::TileVersion::PICTURE_PILE_MODE: {
        gfx::RectF texture_rect = iter.texture_rect();
        gfx::Rect opaque_rect = iter->opaque_rect();
        opaque_rect.Intersect(geometry_rect);

        ResourceProvider* resource_provider =
            layer_tree_impl()->resource_provider();
        ResourceFormat format =
            resource_provider->memory_efficient_texture_format();
        scoped_ptr<PictureDrawQuad> quad = PictureDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     opaque_rect,
                     visible_geometry_rect,
                     texture_rect,
                     iter.texture_size(),
                     format,
                     iter->content_rect(),
                     iter->contents_scale(),
                     pile_);
        draw_quad = quad.PassAs<DrawQuad>();
        break;
      }
      case ManagedTileState::TileVersion::SOLID_COLOR_MODE: {
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     visible_geometry_rect,
                     tile_version.get_solid_color(),
                     false);
        draw_quad = quad.PassAs<DrawQuad>();
        break;
      }
    }

    DCHECK(draw_quad);
    quad_sink->MaybeAppend(draw_quad.Pass());

    if (seen_tilings.empty() || seen_tilings.back() != iter.CurrentTiling())
      seen_tilings.push_back(iter.CurrentTiling());
  }

  // Aggressively remove any tilings that are not seen to save memory. Note
  // that this is at the expense of doing cause more frequent re-painting. A
  // better scheme would be to maintain a tighter visible_content_rect for the
  // finer tilings.
  CleanUpTilingsOnActiveLayer(seen_tilings);
}

void PictureLayerImpl::DidUnregisterLayer() {
  layer_needs_to_register_itself_ = true;
}

void PictureLayerImpl::UpdateTilePriorities() {
  DCHECK(!needs_post_commit_initialization_);
  CHECK(should_update_tile_priorities_);

  if (layer_needs_to_register_itself_) {
    layer_tree_impl()->tile_manager()->RegisterPictureLayerImpl(this);
    layer_needs_to_register_itself_ = false;
  }

  if (!layer_tree_impl()->device_viewport_valid_for_tile_management()) {
    for (size_t i = 0; i < tilings_->num_tilings(); ++i)
      DCHECK(tilings_->tiling_at(i)->has_ever_been_updated());
    return;
  }

  if (!tilings_->num_tilings())
    return;

  double current_frame_time_in_seconds =
      (layer_tree_impl()->CurrentFrameTimeTicks() -
       base::TimeTicks()).InSecondsF();

  bool tiling_needs_update = false;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    if (tilings_->tiling_at(i)->NeedsUpdateForFrameAtTime(
            current_frame_time_in_seconds)) {
      tiling_needs_update = true;
      break;
    }
  }
  if (!tiling_needs_update)
    return;

  UpdateLCDTextStatus(can_use_lcd_text());

  // Use visible_content_rect, unless it's empty. If it's empty, then
  // try to inverse project the viewport into layer space and use that.
  gfx::Rect visible_rect_in_content_space = visible_content_rect();
  if (visible_rect_in_content_space.IsEmpty()) {
    gfx::Transform screen_to_layer(gfx::Transform::kSkipInitialization);
    if (screen_space_transform().GetInverse(&screen_to_layer)) {
      gfx::Size viewport_size = layer_tree_impl()->DrawViewportSize();
      visible_rect_in_content_space =
          gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
              screen_to_layer, gfx::Rect(viewport_size)));
      visible_rect_in_content_space.Intersect(gfx::Rect(content_bounds()));
    }
  }

  WhichTree tree =
      layer_tree_impl()->IsActiveTree() ? ACTIVE_TREE : PENDING_TREE;

  tilings_->UpdateTilePriorities(tree,
                                 visible_rect_in_content_space,
                                 contents_scale_x(),
                                 current_frame_time_in_seconds);

  if (layer_tree_impl()->IsPendingTree())
    MarkVisibleResourcesAsRequired();

  // Tile priorities were modified.
  layer_tree_impl()->DidModifyTilePriorities();
}

void PictureLayerImpl::DidBecomeActive() {
  LayerImpl::DidBecomeActive();
  tilings_->DidBecomeActive();
  layer_tree_impl()->DidModifyTilePriorities();
}

void PictureLayerImpl::DidBeginTracing() {
  pile_->DidBeginTracing();
}

void PictureLayerImpl::ReleaseResources() {
  if (tilings_)
    RemoveAllTilings();

  ResetRasterScale();

  // To avoid an edge case after lost context where the tree is up to date but
  // the tilings have not been managed, request an update draw properties
  // to force tilings to get managed.
  layer_tree_impl()->set_needs_update_draw_properties();
}

void PictureLayerImpl::CalculateContentsScale(
    float ideal_contents_scale,
    float device_scale_factor,
    float page_scale_factor,
    float maximum_animation_contents_scale,
    bool animating_transform_to_screen,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  DoPostCommitInitializationIfNeeded();

  // This function sets valid raster scales and manages tilings, so tile
  // priorities can now be updated.
  should_update_tile_priorities_ = true;

  if (!CanHaveTilings()) {
    ideal_page_scale_ = page_scale_factor;
    ideal_device_scale_ = device_scale_factor;
    ideal_contents_scale_ = ideal_contents_scale;
    ideal_source_scale_ =
        ideal_contents_scale_ / ideal_page_scale_ / ideal_device_scale_;
    *contents_scale_x = ideal_contents_scale_;
    *contents_scale_y = ideal_contents_scale_;
    *content_bounds = gfx::ToCeiledSize(gfx::ScaleSize(bounds(),
                                                       ideal_contents_scale_,
                                                       ideal_contents_scale_));
    return;
  }

  float min_contents_scale = MinimumContentsScale();
  DCHECK_GT(min_contents_scale, 0.f);
  float min_page_scale = layer_tree_impl()->min_page_scale_factor();
  DCHECK_GT(min_page_scale, 0.f);
  float min_device_scale = 1.f;
  float min_source_scale =
      min_contents_scale / min_page_scale / min_device_scale;

  float ideal_page_scale = page_scale_factor;
  float ideal_device_scale = device_scale_factor;
  float ideal_source_scale =
      ideal_contents_scale / ideal_page_scale / ideal_device_scale;

  ideal_contents_scale_ = std::max(ideal_contents_scale, min_contents_scale);
  ideal_page_scale_ = ideal_page_scale;
  ideal_device_scale_ = ideal_device_scale;
  ideal_source_scale_ = std::max(ideal_source_scale, min_source_scale);

  ManageTilings(animating_transform_to_screen,
                maximum_animation_contents_scale);

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

void PictureLayerImpl::SetUseGpuRasterization(bool use_gpu) {
  if (use_gpu_rasterization_ == use_gpu)
    return;

  use_gpu_rasterization_ = use_gpu;
  RemoveAllTilings();
}

scoped_refptr<Tile> PictureLayerImpl::CreateTile(PictureLayerTiling* tiling,
                                               const gfx::Rect& content_rect) {
  if (!pile_->CanRaster(tiling->contents_scale(), content_rect))
    return scoped_refptr<Tile>();

  int flags = 0;
  if (is_using_lcd_text_)
    flags |= Tile::USE_LCD_TEXT;
  if (ShouldUseGpuRasterization())
    flags |= Tile::USE_GPU_RASTERIZATION;
  return layer_tree_impl()->tile_manager()->CreateTile(
      pile_.get(),
      content_rect.size(),
      content_rect,
      contents_opaque() ? content_rect : gfx::Rect(),
      tiling->contents_scale(),
      id(),
      layer_tree_impl()->source_frame_number(),
      flags);
}

void PictureLayerImpl::UpdatePile(Tile* tile) {
  tile->set_picture_pile(pile_);
}

const Region* PictureLayerImpl::GetInvalidation() {
  return &invalidation_;
}

const PictureLayerTiling* PictureLayerImpl::GetTwinTiling(
    const PictureLayerTiling* tiling) const {

  if (!twin_layer_ ||
      twin_layer_->ShouldUseGpuRasterization() != ShouldUseGpuRasterization())
    return NULL;
  for (size_t i = 0; i < twin_layer_->tilings_->num_tilings(); ++i)
    if (twin_layer_->tilings_->tiling_at(i)->contents_scale() ==
        tiling->contents_scale())
      return twin_layer_->tilings_->tiling_at(i);
  return NULL;
}

size_t PictureLayerImpl::GetMaxTilesForInterestArea() const {
  return layer_tree_impl()->settings().max_tiles_for_interest_area;
}

float PictureLayerImpl::GetSkewportTargetTimeInSeconds() const {
  return layer_tree_impl()->settings().skewport_target_time_in_seconds;
}

int PictureLayerImpl::GetSkewportExtrapolationLimitInContentPixels() const {
  return layer_tree_impl()
      ->settings()
      .skewport_extrapolation_limit_in_content_pixels;
}

gfx::Size PictureLayerImpl::CalculateTileSize(
    const gfx::Size& content_bounds) const {
  if (is_mask_) {
    int max_size = layer_tree_impl()->MaxTextureSize();
    return gfx::Size(
        std::min(max_size, content_bounds.width()),
        std::min(max_size, content_bounds.height()));
  }

  int max_texture_size =
      layer_tree_impl()->resource_provider()->max_texture_size();

  gfx::Size default_tile_size = layer_tree_impl()->settings().default_tile_size;
  default_tile_size.SetToMin(gfx::Size(max_texture_size, max_texture_size));

  gfx::Size max_untiled_content_size =
      layer_tree_impl()->settings().max_untiled_layer_size;
  max_untiled_content_size.SetToMin(
      gfx::Size(max_texture_size, max_texture_size));

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

void PictureLayerImpl::SyncFromActiveLayer(const PictureLayerImpl* other) {
  DCHECK(!other->needs_post_commit_initialization_);
  DCHECK(other->tilings_);

  UpdateLCDTextStatus(other->is_using_lcd_text_);

  if (!DrawsContent()) {
    RemoveAllTilings();
    return;
  }

  raster_page_scale_ = other->raster_page_scale_;
  raster_device_scale_ = other->raster_device_scale_;
  raster_source_scale_ = other->raster_source_scale_;
  raster_contents_scale_ = other->raster_contents_scale_;
  low_res_raster_contents_scale_ = other->low_res_raster_contents_scale_;

  // Add synthetic invalidations for any recordings that were dropped.  As
  // tiles are updated to point to this new pile, this will force the dropping
  // of tiles that can no longer be rastered.  This is not ideal, but is a
  // trade-off for memory (use the same pile as much as possible, by switching
  // during DidBecomeActive) and for time (don't bother checking every tile
  // during activation to see if the new pile can still raster it).
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

  // Union in the other newly exposed regions as invalid.
  Region difference_region = Region(gfx::Rect(bounds()));
  difference_region.Subtract(gfx::Rect(other->bounds()));
  invalidation_.Union(difference_region);

  if (CanHaveTilings()) {
    tilings_->SyncTilings(
        *other->tilings_, bounds(), invalidation_, MinimumContentsScale());
  } else {
    RemoveAllTilings();
  }

  SanityCheckTilingState();
}

void PictureLayerImpl::SyncTiling(
    const PictureLayerTiling* tiling) {
  if (!CanHaveTilingWithScale(tiling->contents_scale()))
    return;
  tilings_->AddTiling(tiling->contents_scale());

  if (!layer_tree_impl()->needs_update_draw_properties()) {
    // When the tree is up to date, the set of tilings must either be empty or
    // contain at least one high resolution tiling.  (If it is up to date,
    // then it would be invalid to sync a tiling if it is the first tiling
    // on the layer, since there would be no high resolution tiling.)
    SanityCheckTilingState();
    // TODO(enne): temporary sanity CHECK for http://crbug.com/358350
    CHECK_GT(tilings_->num_tilings(), 1u);
  }

  // If this tree needs update draw properties, then the tiling will
  // get updated prior to drawing or activation.  If this tree does not
  // need update draw properties, then its transforms are up to date and
  // we can create tiles for this tiling immediately.
  if (!layer_tree_impl()->needs_update_draw_properties() &&
      should_update_tile_priorities_)
    UpdateTilePriorities();
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
  PictureLayerTilingSet::CoverageIterator iter(
      tilings_.get(), scale, content_rect, ideal_contents_scale_);

  // Mask resource not ready yet.
  if (!iter || !*iter)
    return 0;

  // Masks only supported if they fit on exactly one tile.
  if (iter.geometry_rect() != content_rect)
    return 0;

  const ManagedTileState::TileVersion& tile_version =
      iter->GetTileVersionForDrawing();
  if (!tile_version.IsReadyToDraw() ||
      tile_version.mode() != ManagedTileState::TileVersion::RESOURCE_MODE)
    return 0;

  return tile_version.get_resource_id();
}

void PictureLayerImpl::MarkVisibleResourcesAsRequired() const {
  DCHECK(layer_tree_impl()->IsPendingTree());
  DCHECK(!layer_tree_impl()->needs_update_draw_properties());
  DCHECK(ideal_contents_scale_);
  DCHECK_GT(tilings_->num_tilings(), 0u);

  // The goal of this function is to find the minimum set of tiles that need to
  // be ready to draw in order to activate without flashing content from a
  // higher res on the active tree to a lower res on the pending tree.

  gfx::Rect rect(visible_content_rect());

  float min_acceptable_scale =
      std::min(raster_contents_scale_, ideal_contents_scale_);

  if (PictureLayerImpl* twin = twin_layer_) {
    float twin_min_acceptable_scale =
        std::min(twin->ideal_contents_scale_, twin->raster_contents_scale_);
    // Ignore 0 scale in case CalculateContentsScale() has never been
    // called for active twin.
    if (twin_min_acceptable_scale != 0.0f) {
      min_acceptable_scale =
          std::min(min_acceptable_scale, twin_min_acceptable_scale);
    }
  }

  PictureLayerTiling* high_res = NULL;
  PictureLayerTiling* low_res = NULL;

  // First pass: ready to draw tiles in acceptable but non-ideal tilings are
  // marked as required for activation so that their textures are not thrown
  // away; any non-ready tiles are not marked as required.
  Region missing_region = rect;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    DCHECK(tiling->has_ever_been_updated());

    if (tiling->resolution() == LOW_RESOLUTION) {
      DCHECK(!low_res) << "There can only be one low res tiling";
      low_res = tiling;
    }
    if (tiling->contents_scale() < min_acceptable_scale)
      continue;
    if (tiling->resolution() == HIGH_RESOLUTION) {
      DCHECK(!high_res) << "There can only be one high res tiling";
      high_res = tiling;
      continue;
    }
    for (PictureLayerTiling::CoverageIterator iter(tiling,
                                                   contents_scale_x(),
                                                   rect);
         iter;
         ++iter) {
      if (!*iter || !iter->IsReadyToDraw())
        continue;

      missing_region.Subtract(iter.geometry_rect());
      iter->MarkRequiredForActivation();
    }
  }
  DCHECK(high_res) << "There must be one high res tiling";

  // If these pointers are null (because no twin, no matching tiling, or the
  // simpification just below), then high res tiles will be required to fill any
  // holes left by the first pass above.  If the pointers are valid, then this
  // layer is allowed to skip any tiles that are not ready on its twin.
  const PictureLayerTiling* twin_high_res = NULL;
  const PictureLayerTiling* twin_low_res = NULL;

  // As a simplification, only allow activating to skip twin tiles that the
  // active layer is also missing when both this layer and its twin have 2
  // tilings (high and low).  This avoids having to iterate/track coverage of
  // non-ideal tilings during the last draw call on the active layer.
  if (high_res && low_res && tilings_->num_tilings() == 2 &&
      twin_layer_ && twin_layer_->tilings_->num_tilings() == 2) {
    twin_low_res = GetTwinTiling(low_res);
    if (twin_low_res)
      twin_high_res = GetTwinTiling(high_res);
  }
  // If this layer and its twin have different transforms, then don't compare
  // them and only allow activating to high res tiles, since tiles on each layer
  // will be in different places on screen.
  if (!twin_high_res || !twin_low_res ||
      twin_layer_->layer_tree_impl()->RequiresHighResToDraw() ||
      draw_properties().screen_space_transform !=
          twin_layer_->draw_properties().screen_space_transform) {
    twin_high_res = NULL;
    twin_low_res = NULL;
  }

  // As a second pass, mark as required any visible high res tiles not filled in
  // by acceptable non-ideal tiles from the first pass.
  if (MarkVisibleTilesAsRequired(
      high_res, twin_high_res, contents_scale_x(), rect, missing_region)) {
    // As an optional third pass, if a high res tile was skipped because its
    // twin was also missing, then fall back to mark low res tiles as required
    // in case the active twin is substituting those for missing high res
    // content.
    MarkVisibleTilesAsRequired(
        low_res, twin_low_res, contents_scale_x(), rect, missing_region);
  }
}

bool PictureLayerImpl::MarkVisibleTilesAsRequired(
    PictureLayerTiling* tiling,
    const PictureLayerTiling* optional_twin_tiling,
    float contents_scale,
    const gfx::Rect& rect,
    const Region& missing_region) const {
  bool twin_had_missing_tile = false;
  for (PictureLayerTiling::CoverageIterator iter(tiling,
                                                 contents_scale,
                                                 rect);
       iter;
       ++iter) {
    Tile* tile = *iter;
    // A null tile (i.e. missing recording) can just be skipped.
    if (!tile)
      continue;

    // If the missing region doesn't cover it, this tile is fully
    // covered by acceptable tiles at other scales.
    if (!missing_region.Intersects(iter.geometry_rect()))
      continue;

    // If the twin tile doesn't exist (i.e. missing recording or so far away
    // that it is outside the visible tile rect) or this tile is shared between
    // with the twin, then this tile isn't required to prevent flashing.
    if (optional_twin_tiling) {
      Tile* twin_tile = optional_twin_tiling->TileAt(iter.i(), iter.j());
      if (!twin_tile || twin_tile == tile) {
        twin_had_missing_tile = true;
        continue;
      }
    }

    tile->MarkRequiredForActivation();
  }
  return twin_had_missing_tile;
}

void PictureLayerImpl::DoPostCommitInitialization() {
  DCHECK(needs_post_commit_initialization_);
  DCHECK(layer_tree_impl()->IsPendingTree());

  if (!tilings_)
    tilings_.reset(new PictureLayerTilingSet(this, bounds()));

  DCHECK(!twin_layer_);
  twin_layer_ = static_cast<PictureLayerImpl*>(
      layer_tree_impl()->FindActiveTreeLayerById(id()));
  if (twin_layer_) {
    DCHECK(!twin_layer_->twin_layer_);
    twin_layer_->twin_layer_ = this;
    // If the twin has never been pushed to, do not sync from it.
    // This can happen if this function is called during activation.
    if (!twin_layer_->needs_post_commit_initialization_)
      SyncFromActiveLayer(twin_layer_);
  }

  needs_post_commit_initialization_ = false;
}

PictureLayerTiling* PictureLayerImpl::AddTiling(float contents_scale) {
  DCHECK(CanHaveTilingWithScale(contents_scale)) <<
      "contents_scale: " << contents_scale;

  PictureLayerTiling* tiling = tilings_->AddTiling(contents_scale);

  DCHECK(pile_->HasRecordings());

  if (twin_layer_ &&
      twin_layer_->ShouldUseGpuRasterization() == ShouldUseGpuRasterization())
    twin_layer_->SyncTiling(tiling);

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
  if (tilings_->num_tilings() == 0)
    ResetRasterScale();
  SanityCheckTilingState();
}

void PictureLayerImpl::RemoveAllTilings() {
  if (tilings_)
    tilings_->RemoveAllTilings();
  // If there are no tilings, then raster scales are no longer meaningful.
  ResetRasterScale();
}

namespace {

inline float PositiveRatio(float float1, float float2) {
  DCHECK_GT(float1, 0);
  DCHECK_GT(float2, 0);
  return float1 > float2 ? float1 / float2 : float2 / float1;
}

}  // namespace

void PictureLayerImpl::ManageTilings(bool animating_transform_to_screen,
                                     float maximum_animation_contents_scale) {
  DCHECK(ideal_contents_scale_);
  DCHECK(ideal_page_scale_);
  DCHECK(ideal_device_scale_);
  DCHECK(ideal_source_scale_);
  DCHECK(CanHaveTilings());
  DCHECK(!needs_post_commit_initialization_);

  bool change_target_tiling =
      raster_page_scale_ == 0.f ||
      raster_device_scale_ == 0.f ||
      raster_source_scale_ == 0.f ||
      raster_contents_scale_ == 0.f ||
      low_res_raster_contents_scale_ == 0.f ||
      ShouldAdjustRasterScale(animating_transform_to_screen);

  if (tilings_->num_tilings() == 0) {
    DCHECK(change_target_tiling)
        << "A layer with no tilings shouldn't have valid raster scales";
  }

  // Store the value for the next time ShouldAdjustRasterScale is called.
  raster_source_scale_was_animating_ = animating_transform_to_screen;

  if (!change_target_tiling)
    return;

  if (!layer_tree_impl()->device_viewport_valid_for_tile_management())
    return;

  RecalculateRasterScales(animating_transform_to_screen,
                          maximum_animation_contents_scale);

  PictureLayerTiling* high_res = NULL;
  PictureLayerTiling* low_res = NULL;

  PictureLayerTiling* previous_low_res = NULL;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (tiling->contents_scale() == raster_contents_scale_)
      high_res = tiling;
    if (tiling->contents_scale() == low_res_raster_contents_scale_)
      low_res = tiling;
    if (tiling->resolution() == LOW_RESOLUTION)
      previous_low_res = tiling;

    // Reset all tilings to non-ideal until the end of this function.
    tiling->set_resolution(NON_IDEAL_RESOLUTION);
  }

  if (!high_res) {
    high_res = AddTiling(raster_contents_scale_);
    if (raster_contents_scale_ == low_res_raster_contents_scale_)
      low_res = high_res;
  }

  // Only create new low res tilings when the transform is static.  This
  // prevents wastefully creating a paired low res tiling for every new high res
  // tiling during a pinch or a CSS animation.
  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (ShouldHaveLowResTiling() && !is_pinching &&
      !animating_transform_to_screen &&
      !low_res && low_res != high_res)
    low_res = AddTiling(low_res_raster_contents_scale_);

  // Set low-res if we have one.
  if (!low_res)
    low_res = previous_low_res;
  if (low_res && low_res != high_res)
    low_res->set_resolution(LOW_RESOLUTION);

  // Make sure we always have one high-res (even if high == low).
  high_res->set_resolution(HIGH_RESOLUTION);

  SanityCheckTilingState();
}

bool PictureLayerImpl::ShouldAdjustRasterScale(
    bool animating_transform_to_screen) const {
  // TODO(danakj): Adjust raster source scale closer to ideal source scale at
  // a throttled rate. Possibly make use of invalidation_.IsEmpty() on pending
  // tree. This will allow CSS scale changes to get re-rastered at an
  // appropriate rate.

  if (raster_source_scale_was_animating_ != animating_transform_to_screen)
    return true;

  if (animating_transform_to_screen &&
      raster_contents_scale_ != ideal_contents_scale_ &&
      ShouldUseGpuRasterization())
    return true;

  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (is_pinching && raster_page_scale_) {
    // We change our raster scale when it is:
    // - Higher than ideal (need a lower-res tiling available)
    // - Too far from ideal (need a higher-res tiling available)
    float ratio = ideal_page_scale_ / raster_page_scale_;
    if (raster_page_scale_ > ideal_page_scale_ ||
        ratio > kMaxScaleRatioDuringPinch)
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

float PictureLayerImpl::SnappedContentsScale(float scale) {
  // If a tiling exists within the max snapping ratio, snap to its scale.
  float snapped_contents_scale = scale;
  float snapped_ratio = kSnapToExistingTilingRatio;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    float tiling_contents_scale = tilings_->tiling_at(i)->contents_scale();
    float ratio = PositiveRatio(tiling_contents_scale, scale);
    if (ratio < snapped_ratio) {
      snapped_contents_scale = tiling_contents_scale;
      snapped_ratio = ratio;
    }
  }
  return snapped_contents_scale;
}

void PictureLayerImpl::RecalculateRasterScales(
    bool animating_transform_to_screen,
    float maximum_animation_contents_scale) {
  raster_device_scale_ = ideal_device_scale_;
  raster_source_scale_ = ideal_source_scale_;

  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (!is_pinching || raster_contents_scale_ == 0.f) {
    // When not pinching or when we have no previous scale, we use ideal scale:
    raster_page_scale_ = ideal_page_scale_;
    raster_contents_scale_ = ideal_contents_scale_;
  } else {
    // See ShouldAdjustRasterScale:
    // - When zooming out, preemptively create new tiling at lower resolution.
    // - When zooming in, approximate ideal using multiple of kMaxScaleRatio.
    bool zooming_out = raster_page_scale_ > ideal_page_scale_;
    float desired_contents_scale =
        zooming_out ? raster_contents_scale_ / kMaxScaleRatioDuringPinch
                    : raster_contents_scale_ * kMaxScaleRatioDuringPinch;
    raster_contents_scale_ = SnappedContentsScale(desired_contents_scale);
    raster_page_scale_ = raster_contents_scale_ / raster_device_scale_;
  }

  raster_contents_scale_ =
      std::max(raster_contents_scale_, MinimumContentsScale());

  // If we're not re-rasterizing during animation, rasterize at the maximum
  // scale that will occur during the animation, if the maximum scale is
  // known.
  if (animating_transform_to_screen && !ShouldUseGpuRasterization()) {
    if (maximum_animation_contents_scale > 0.f) {
      raster_contents_scale_ =
          std::max(raster_contents_scale_, maximum_animation_contents_scale);
    } else {
      raster_contents_scale_ =
          std::max(raster_contents_scale_,
                   1.f * ideal_page_scale_ * ideal_device_scale_);
    }
  }

  // If this layer would only create one tile at this content scale,
  // don't create a low res tiling.
  gfx::Size content_bounds =
      gfx::ToCeiledSize(gfx::ScaleSize(bounds(), raster_contents_scale_));
  gfx::Size tile_size = CalculateTileSize(content_bounds);
  if (tile_size.width() >= content_bounds.width() &&
      tile_size.height() >= content_bounds.height()) {
    low_res_raster_contents_scale_ = raster_contents_scale_;
    return;
  }

  float low_res_factor =
      layer_tree_impl()->settings().low_res_contents_scale_factor;
  low_res_raster_contents_scale_ = std::max(
      raster_contents_scale_ * low_res_factor,
      MinimumContentsScale());
}

void PictureLayerImpl::CleanUpTilingsOnActiveLayer(
    std::vector<PictureLayerTiling*> used_tilings) {
  DCHECK(layer_tree_impl()->IsActiveTree());
  if (tilings_->num_tilings() == 0)
    return;

  float min_acceptable_high_res_scale = std::min(
      raster_contents_scale_, ideal_contents_scale_);
  float max_acceptable_high_res_scale = std::max(
      raster_contents_scale_, ideal_contents_scale_);

  PictureLayerImpl* twin = twin_layer_;
  if (twin) {
    min_acceptable_high_res_scale = std::min(
        min_acceptable_high_res_scale,
        std::min(twin->raster_contents_scale_, twin->ideal_contents_scale_));
    max_acceptable_high_res_scale = std::max(
        max_acceptable_high_res_scale,
        std::max(twin->raster_contents_scale_, twin->ideal_contents_scale_));
  }

  std::vector<PictureLayerTiling*> to_remove;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);

    // Keep multiple high resolution tilings even if not used to help
    // activate earlier at non-ideal resolutions.
    if (tiling->contents_scale() >= min_acceptable_high_res_scale &&
        tiling->contents_scale() <= max_acceptable_high_res_scale)
      continue;

    // Keep low resolution tilings, if the layer should have them.
    if (tiling->resolution() == LOW_RESOLUTION && ShouldHaveLowResTiling())
      continue;

    // Don't remove tilings that are being used (and thus would cause a flash.)
    if (std::find(used_tilings.begin(), used_tilings.end(), tiling) !=
        used_tilings.end())
      continue;

    to_remove.push_back(tiling);
  }

  for (size_t i = 0; i < to_remove.size(); ++i) {
    const PictureLayerTiling* twin_tiling = GetTwinTiling(to_remove[i]);
    // Only remove tilings from the twin layer if they have
    // NON_IDEAL_RESOLUTION.
    if (twin_tiling && twin_tiling->resolution() == NON_IDEAL_RESOLUTION)
      twin->RemoveTiling(to_remove[i]->contents_scale());
    // TODO(enne): temporary sanity CHECK for http://crbug.com/358350
    CHECK_NE(HIGH_RESOLUTION, to_remove[i]->resolution());
    tilings_->Remove(to_remove[i]);
  }
  DCHECK_GT(tilings_->num_tilings(), 0u);

  SanityCheckTilingState();
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

void PictureLayerImpl::UpdateLCDTextStatus(bool new_status) {
  // Once this layer is not using lcd text, don't switch back.
  if (!is_using_lcd_text_)
    return;

  if (is_using_lcd_text_ == new_status)
    return;

  is_using_lcd_text_ = new_status;
  tilings_->SetCanUseLCDText(is_using_lcd_text_);
}

void PictureLayerImpl::ResetRasterScale() {
  raster_page_scale_ = 0.f;
  raster_device_scale_ = 0.f;
  raster_source_scale_ = 0.f;
  raster_contents_scale_ = 0.f;
  low_res_raster_contents_scale_ = 0.f;

  // When raster scales aren't valid, don't update tile priorities until
  // this layer has been updated via UpdateDrawProperties.
  should_update_tile_priorities_ = false;
}

bool PictureLayerImpl::CanHaveTilings() const {
  if (!DrawsContent())
    return false;
  if (!pile_->HasRecordings())
    return false;
  return true;
}

bool PictureLayerImpl::CanHaveTilingWithScale(float contents_scale) const {
  if (!CanHaveTilings())
    return false;
  if (contents_scale < MinimumContentsScale())
    return false;
  return true;
}

void PictureLayerImpl::SanityCheckTilingState() const {
#if DCHECK_IS_ON
  if (!CanHaveTilings()) {
    DCHECK_EQ(0u, tilings_->num_tilings());
    return;
  }
  if (tilings_->num_tilings() == 0)
    return;

  // MarkVisibleResourcesAsRequired depends on having exactly 1 high res
  // tiling to mark its tiles as being required for activation.
  DCHECK_EQ(1, tilings_->NumHighResTilings());
#endif
}

void PictureLayerImpl::GetDebugBorderProperties(
    SkColor* color,
    float* width) const {
  *color = DebugColors::TiledContentLayerBorderColor();
  *width = DebugColors::TiledContentLayerBorderWidth(layer_tree_impl());
}

void PictureLayerImpl::AsValueInto(base::DictionaryValue* state) const {
  const_cast<PictureLayerImpl*>(this)->DoPostCommitInitializationIfNeeded();
  LayerImpl::AsValueInto(state);
  state->SetDouble("ideal_contents_scale", ideal_contents_scale_);
  state->SetDouble("geometry_contents_scale", contents_scale_x());
  state->Set("tilings", tilings_->AsValue().release());
  state->Set("pictures", pile_->AsValue().release());
  state->Set("invalidation", invalidation_.AsValue().release());

  scoped_ptr<base::ListValue> coverage_tiles(new base::ListValue);
  for (PictureLayerTilingSet::CoverageIterator iter(tilings_.get(),
                                                    contents_scale_x(),
                                                    gfx::Rect(content_bounds()),
                                                    ideal_contents_scale_);
       iter;
       ++iter) {
    scoped_ptr<base::DictionaryValue> tile_data(new base::DictionaryValue);
    tile_data->Set("geometry_rect",
                   MathUtil::AsValue(iter.geometry_rect()).release());
    if (*iter)
      tile_data->Set("tile", TracedValue::CreateIDRef(*iter).release());

    coverage_tiles->Append(tile_data.release());
  }
  state->Set("coverage_tiles", coverage_tiles.release());
  state->SetBoolean("is_using_lcd_text", is_using_lcd_text_);
  state->SetBoolean("using_gpu_rasterization", ShouldUseGpuRasterization());
}

size_t PictureLayerImpl::GPUMemoryUsageInBytes() const {
  const_cast<PictureLayerImpl*>(this)->DoPostCommitInitializationIfNeeded();
  return tilings_->GPUMemoryUsageInBytes();
}

void PictureLayerImpl::RunMicroBenchmark(MicroBenchmarkImpl* benchmark) {
  benchmark->RunOnLayer(this);
}

WhichTree PictureLayerImpl::GetTree() const {
  return layer_tree_impl()->IsActiveTree() ? ACTIVE_TREE : PENDING_TREE;
}

bool PictureLayerImpl::IsOnActiveOrPendingTree() const {
  return !layer_tree_impl()->IsRecycleTree();
}

PictureLayerImpl::LayerRasterTileIterator::LayerRasterTileIterator()
    : layer_(NULL) {}

PictureLayerImpl::LayerRasterTileIterator::LayerRasterTileIterator(
    PictureLayerImpl* layer,
    bool prioritize_low_res)
    : layer_(layer), current_stage_(0) {
  DCHECK(layer_);
  if (!layer_->tilings_ || !layer_->tilings_->num_tilings()) {
    current_stage_ = arraysize(stages_);
    return;
  }

  WhichTree tree =
      layer_->layer_tree_impl()->IsActiveTree() ? ACTIVE_TREE : PENDING_TREE;

  // Find high and low res tilings and initialize the iterators.
  for (size_t i = 0; i < layer_->tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = layer_->tilings_->tiling_at(i);
    if (tiling->resolution() == HIGH_RESOLUTION) {
      iterators_[HIGH_RES] =
          PictureLayerTiling::TilingRasterTileIterator(tiling, tree);
    }

    if (tiling->resolution() == LOW_RESOLUTION) {
      iterators_[LOW_RES] =
          PictureLayerTiling::TilingRasterTileIterator(tiling, tree);
    }
  }

  if (prioritize_low_res) {
    stages_[0].iterator_type = LOW_RES;
    stages_[0].tile_type = TilePriority::NOW;

    stages_[1].iterator_type = HIGH_RES;
    stages_[1].tile_type = TilePriority::NOW;
  } else {
    stages_[0].iterator_type = HIGH_RES;
    stages_[0].tile_type = TilePriority::NOW;

    stages_[1].iterator_type = LOW_RES;
    stages_[1].tile_type = TilePriority::NOW;
  }

  stages_[2].iterator_type = HIGH_RES;
  stages_[2].tile_type = TilePriority::SOON;

  stages_[3].iterator_type = HIGH_RES;
  stages_[3].tile_type = TilePriority::EVENTUALLY;

  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;
  if (!iterators_[index] || iterators_[index].get_type() != tile_type)
    ++(*this);
}

PictureLayerImpl::LayerRasterTileIterator::~LayerRasterTileIterator() {}

PictureLayerImpl::LayerRasterTileIterator::operator bool() const {
  return layer_ && static_cast<size_t>(current_stage_) < arraysize(stages_);
}

PictureLayerImpl::LayerRasterTileIterator&
PictureLayerImpl::LayerRasterTileIterator::
operator++() {
  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

  // First advance the iterator.
  if (iterators_[index])
    ++iterators_[index];

  if (iterators_[index] && iterators_[index].get_type() == tile_type)
    return *this;

  // Next, advance the stage.
  int stage_count = arraysize(stages_);
  ++current_stage_;
  while (current_stage_ < stage_count) {
    index = stages_[current_stage_].iterator_type;
    tile_type = stages_[current_stage_].tile_type;

    if (iterators_[index] && iterators_[index].get_type() == tile_type)
      break;
    ++current_stage_;
  }
  return *this;
}

Tile* PictureLayerImpl::LayerRasterTileIterator::operator*() {
  DCHECK(*this);

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(iterators_[index]);
  DCHECK(iterators_[index].get_type() == stages_[current_stage_].tile_type);

  return *iterators_[index];
}

PictureLayerImpl::LayerEvictionTileIterator::LayerEvictionTileIterator()
    : iterator_index_(0),
      iteration_stage_(TilePriority::EVENTUALLY),
      required_for_activation_(false),
      layer_(NULL) {}

PictureLayerImpl::LayerEvictionTileIterator::LayerEvictionTileIterator(
    PictureLayerImpl* layer,
    TreePriority tree_priority)
    : iterator_index_(0),
      iteration_stage_(TilePriority::EVENTUALLY),
      required_for_activation_(false),
      layer_(layer) {
  if (!layer_->tilings_ || !layer_->tilings_->num_tilings())
    return;

  size_t high_res_tiling_index = layer_->tilings_->num_tilings();
  size_t low_res_tiling_index = layer_->tilings_->num_tilings();
  for (size_t i = 0; i < layer_->tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = layer_->tilings_->tiling_at(i);
    if (tiling->resolution() == HIGH_RESOLUTION)
      high_res_tiling_index = i;
    else if (tiling->resolution() == LOW_RESOLUTION)
      low_res_tiling_index = i;
  }

  iterators_.reserve(layer_->tilings_->num_tilings());

  // Higher resolution non-ideal goes first.
  for (size_t i = 0; i < high_res_tiling_index; ++i) {
    iterators_.push_back(PictureLayerTiling::TilingEvictionTileIterator(
        layer_->tilings_->tiling_at(i), tree_priority));
  }

  // Lower resolution non-ideal goes next.
  for (size_t i = layer_->tilings_->num_tilings() - 1;
       i > high_res_tiling_index;
       --i) {
    PictureLayerTiling* tiling = layer_->tilings_->tiling_at(i);
    if (tiling->resolution() == LOW_RESOLUTION)
      continue;

    iterators_.push_back(
        PictureLayerTiling::TilingEvictionTileIterator(tiling, tree_priority));
  }

  // Now, put the low res tiling if we have one.
  if (low_res_tiling_index < layer_->tilings_->num_tilings()) {
    iterators_.push_back(PictureLayerTiling::TilingEvictionTileIterator(
        layer_->tilings_->tiling_at(low_res_tiling_index), tree_priority));
  }

  // Finally, put the high res tiling if we have one.
  if (high_res_tiling_index < layer_->tilings_->num_tilings()) {
    iterators_.push_back(PictureLayerTiling::TilingEvictionTileIterator(
        layer_->tilings_->tiling_at(high_res_tiling_index), tree_priority));
  }

  DCHECK_GT(iterators_.size(), 0u);

  if (!iterators_[iterator_index_] ||
      !IsCorrectType(&iterators_[iterator_index_])) {
    AdvanceToNextIterator();
  }
}

PictureLayerImpl::LayerEvictionTileIterator::~LayerEvictionTileIterator() {}

Tile* PictureLayerImpl::LayerEvictionTileIterator::operator*() {
  DCHECK(*this);
  return *iterators_[iterator_index_];
}

PictureLayerImpl::LayerEvictionTileIterator&
PictureLayerImpl::LayerEvictionTileIterator::
operator++() {
  DCHECK(*this);
  ++iterators_[iterator_index_];
  if (!iterators_[iterator_index_] ||
      !IsCorrectType(&iterators_[iterator_index_])) {
    AdvanceToNextIterator();
  }
  return *this;
}

void PictureLayerImpl::LayerEvictionTileIterator::AdvanceToNextIterator() {
  ++iterator_index_;

  while (true) {
    while (iterator_index_ < iterators_.size()) {
      if (iterators_[iterator_index_] &&
          IsCorrectType(&iterators_[iterator_index_])) {
        return;
      }
      ++iterator_index_;
    }

    // If we're NOW and required_for_activation, then this was the last pass
    // through the iterators.
    if (iteration_stage_ == TilePriority::NOW && required_for_activation_)
      break;

    if (!required_for_activation_) {
      required_for_activation_ = true;
    } else {
      required_for_activation_ = false;
      iteration_stage_ =
          static_cast<TilePriority::PriorityBin>(iteration_stage_ - 1);
    }
    iterator_index_ = 0;
  }
}

PictureLayerImpl::LayerEvictionTileIterator::operator bool() const {
  return iterator_index_ < iterators_.size();
}

bool PictureLayerImpl::LayerEvictionTileIterator::IsCorrectType(
    PictureLayerTiling::TilingEvictionTileIterator* it) const {
  return it->get_type() == iteration_stage_ &&
         (**it)->required_for_activation() == required_for_activation_;
}

}  // namespace cc
