// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include <algorithm>
#include <limits>
#include <set>

#include "base/debug/trace_event_argument.h"
#include "base/time/time.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/micro_benchmark_impl.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/output/begin_frame_args.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/tile_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace {
const float kMaxScaleRatioDuringPinch = 2.0f;

// When creating a new tiling during pinch, snap to an existing
// tiling's scale if the desired scale is within this ratio.
const float kSnapToExistingTilingRatio = 1.2f;

// Estimate skewport 60 frames ahead for pre-rasterization on the CPU.
const float kCpuSkewportTargetTimeInFrames = 60.0f;

// Don't pre-rasterize on the GPU (except for kBackflingGuardDistancePixels in
// TileManager::BinFromTilePriority).
const float kGpuSkewportTargetTimeInFrames = 0.0f;

// Even for really wide viewports, at some point GPU raster should use
// less than 4 tiles to fill the viewport. This is set to 256 as a
// sane minimum for now, but we might want to tune this for low-end.
const int kMinHeightForGpuRasteredTile = 256;

// When making odd-sized tiles, round them up to increase the chances
// of using the same tile size.
const int kTileRoundUp = 64;

}  // namespace

namespace cc {

PictureLayerImpl::Pair::Pair() : active(nullptr), pending(nullptr) {
}

PictureLayerImpl::Pair::Pair(PictureLayerImpl* active_layer,
                             PictureLayerImpl* pending_layer)
    : active(active_layer), pending(pending_layer) {
}

PictureLayerImpl::Pair::~Pair() {
}

PictureLayerImpl::PictureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      twin_layer_(nullptr),
      pile_(PicturePileImpl::Create()),
      ideal_page_scale_(0.f),
      ideal_device_scale_(0.f),
      ideal_source_scale_(0.f),
      ideal_contents_scale_(0.f),
      raster_page_scale_(0.f),
      raster_device_scale_(0.f),
      raster_source_scale_(0.f),
      raster_contents_scale_(0.f),
      low_res_raster_contents_scale_(0.f),
      raster_source_scale_is_fixed_(false),
      was_screen_space_transform_animating_(false),
      needs_post_commit_initialization_(true),
      should_update_tile_priorities_(false),
      only_used_low_res_last_append_quads_(false) {
  layer_tree_impl()->RegisterPictureLayerImpl(this);
}

PictureLayerImpl::~PictureLayerImpl() {
  if (twin_layer_)
    twin_layer_->twin_layer_ = nullptr;
  layer_tree_impl()->UnregisterPictureLayerImpl(this);
}

const char* PictureLayerImpl::LayerTypeAsString() const {
  return "cc::PictureLayerImpl";
}

scoped_ptr<LayerImpl> PictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id());
}

void PictureLayerImpl::PushPropertiesTo(LayerImpl* base_layer) {
  // It's possible this layer was never drawn or updated (e.g. because it was
  // a descendant of an opacity 0 layer).
  DoPostCommitInitializationIfNeeded();
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  LayerImpl::PushPropertiesTo(base_layer);

  // Twin relationships should never change once established.
  DCHECK_IMPLIES(twin_layer_, twin_layer_ == layer_impl);
  DCHECK_IMPLIES(twin_layer_, layer_impl->twin_layer_ == this);
  // The twin relationship does not need to exist before the first
  // PushPropertiesTo from pending to active layer since before that the active
  // layer can not have a pile or tilings, it has only been created and inserted
  // into the tree at that point.
  twin_layer_ = layer_impl;
  layer_impl->twin_layer_ = this;

  layer_impl->UpdatePile(pile_);

  DCHECK(!pile_->is_solid_color() || !tilings_->num_tilings());
  // Tilings would be expensive to push, so we swap.
  layer_impl->tilings_.swap(tilings_);
  layer_impl->tilings_->SetClient(layer_impl);
  if (tilings_)
    tilings_->SetClient(this);

  // Ensure that the recycle tree doesn't have any unshared tiles.
  if (tilings_ && pile_->is_solid_color())
    tilings_->RemoveAllTilings();

  // Remove invalidated tiles from what will become a recycle tree.
  if (tilings_)
    tilings_->RemoveTilesInRegion(invalidation_);

  layer_impl->raster_page_scale_ = raster_page_scale_;
  layer_impl->raster_device_scale_ = raster_device_scale_;
  layer_impl->raster_source_scale_ = raster_source_scale_;
  layer_impl->raster_contents_scale_ = raster_contents_scale_;
  layer_impl->low_res_raster_contents_scale_ = low_res_raster_contents_scale_;
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

void PictureLayerImpl::UpdatePile(scoped_refptr<PicturePileImpl> pile) {
  bool could_have_tilings = CanHaveTilings();
  pile_.swap(pile);

  // Need to call UpdateTiles again if CanHaveTilings changed.
  if (could_have_tilings != CanHaveTilings()) {
    layer_tree_impl()->set_needs_update_draw_properties();
  }
}

void PictureLayerImpl::AppendQuads(RenderPass* render_pass,
                                   const Occlusion& occlusion_in_content_space,
                                   AppendQuadsData* append_quads_data) {
  DCHECK(!needs_post_commit_initialization_);
  // The bounds and the pile size may differ if the pile wasn't updated (ie.
  // PictureLayer::Update didn't happen). But that should never be the case if
  // the layer is part of the visible frame, which is why we're appending quads
  // in the first place
  DCHECK_EQ(bounds().ToString(), pile_->tiling_size().ToString());

  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();

  if (pile_->is_solid_color()) {
    PopulateSharedQuadState(shared_quad_state);

    AppendDebugBorderQuad(
        render_pass, bounds(), shared_quad_state, append_quads_data);

    SolidColorLayerImpl::AppendSolidQuads(render_pass,
                                          occlusion_in_content_space,
                                          shared_quad_state,
                                          visible_content_rect(),
                                          pile_->solid_color(),
                                          append_quads_data);
    return;
  }

  float max_contents_scale = MaximumTilingContentsScale();
  gfx::Transform scaled_draw_transform = draw_transform();
  scaled_draw_transform.Scale(SK_MScalar1 / max_contents_scale,
                              SK_MScalar1 / max_contents_scale);
  gfx::Size scaled_content_bounds =
      gfx::ToCeiledSize(gfx::ScaleSize(bounds(), max_contents_scale));
  gfx::Rect scaled_visible_content_rect =
      gfx::ScaleToEnclosingRect(visible_content_rect(), max_contents_scale);
  scaled_visible_content_rect.Intersect(gfx::Rect(scaled_content_bounds));
  Occlusion scaled_occlusion =
      occlusion_in_content_space.GetOcclusionWithGivenDrawTransform(
          scaled_draw_transform);

  shared_quad_state->SetAll(scaled_draw_transform,
                            scaled_content_bounds,
                            scaled_visible_content_rect,
                            draw_properties().clip_rect,
                            draw_properties().is_clipped,
                            draw_properties().opacity,
                            blend_mode(),
                            sorting_context_id_);

  if (current_draw_mode_ == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    AppendDebugBorderQuad(
        render_pass,
        scaled_content_bounds,
        shared_quad_state,
        append_quads_data,
        DebugColors::DirectPictureBorderColor(),
        DebugColors::DirectPictureBorderWidth(layer_tree_impl()));

    gfx::Rect geometry_rect = scaled_visible_content_rect;
    gfx::Rect opaque_rect = contents_opaque() ? geometry_rect : gfx::Rect();
    gfx::Rect visible_geometry_rect =
        scaled_occlusion.GetUnoccludedContentRect(geometry_rect);
    if (visible_geometry_rect.IsEmpty())
      return;

    gfx::Size texture_size = scaled_visible_content_rect.size();
    gfx::RectF texture_rect = gfx::RectF(texture_size);
    gfx::Rect quad_content_rect = scaled_visible_content_rect;

    PictureDrawQuad* quad =
        render_pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
    quad->SetNew(shared_quad_state,
                 geometry_rect,
                 opaque_rect,
                 visible_geometry_rect,
                 texture_rect,
                 texture_size,
                 RGBA_8888,
                 quad_content_rect,
                 max_contents_scale,
                 pile_);
    return;
  }

  AppendDebugBorderQuad(
      render_pass, scaled_content_bounds, shared_quad_state, append_quads_data);

  if (ShowDebugBorders()) {
    for (PictureLayerTilingSet::CoverageIterator iter(
             tilings_.get(),
             max_contents_scale,
             scaled_visible_content_rect,
             ideal_contents_scale_);
         iter;
         ++iter) {
      SkColor color;
      float width;
      if (*iter && iter->IsReadyToDraw()) {
        ManagedTileState::DrawInfo::Mode mode = iter->draw_info().mode();
        if (mode == ManagedTileState::DrawInfo::SOLID_COLOR_MODE) {
          color = DebugColors::SolidColorTileBorderColor();
          width = DebugColors::SolidColorTileBorderWidth(layer_tree_impl());
        } else if (mode == ManagedTileState::DrawInfo::PICTURE_PILE_MODE) {
          color = DebugColors::PictureTileBorderColor();
          width = DebugColors::PictureTileBorderWidth(layer_tree_impl());
        } else if (iter.resolution() == HIGH_RESOLUTION) {
          color = DebugColors::HighResTileBorderColor();
          width = DebugColors::HighResTileBorderWidth(layer_tree_impl());
        } else if (iter.resolution() == LOW_RESOLUTION) {
          color = DebugColors::LowResTileBorderColor();
          width = DebugColors::LowResTileBorderWidth(layer_tree_impl());
        } else if (iter->contents_scale() > max_contents_scale) {
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

      DebugBorderDrawQuad* debug_border_quad =
          render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
      gfx::Rect geometry_rect = iter.geometry_rect();
      gfx::Rect visible_geometry_rect = geometry_rect;
      debug_border_quad->SetNew(shared_quad_state,
                                geometry_rect,
                                visible_geometry_rect,
                                color,
                                width);
    }
  }

  // Keep track of the tilings that were used so that tilings that are
  // unused can be considered for removal.
  std::vector<PictureLayerTiling*> seen_tilings;

  // Ignore missing tiles outside of viewport for tile priority. This is
  // normally the same as draw viewport but can be independently overridden by
  // embedders like Android WebView with SetExternalDrawConstraints.
  gfx::Rect scaled_viewport_for_tile_priority = gfx::ScaleToEnclosingRect(
      GetViewportForTilePriorityInContentSpace(), max_contents_scale);

  size_t missing_tile_count = 0u;
  size_t on_demand_missing_tile_count = 0u;
  only_used_low_res_last_append_quads_ = true;
  for (PictureLayerTilingSet::CoverageIterator iter(tilings_.get(),
                                                    max_contents_scale,
                                                    scaled_visible_content_rect,
                                                    ideal_contents_scale_);
       iter;
       ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    gfx::Rect opaque_rect = contents_opaque() ? geometry_rect : gfx::Rect();
    gfx::Rect visible_geometry_rect =
        scaled_occlusion.GetUnoccludedContentRect(geometry_rect);
    if (visible_geometry_rect.IsEmpty())
      continue;

    append_quads_data->visible_content_area +=
        visible_geometry_rect.width() * visible_geometry_rect.height();

    bool has_draw_quad = false;
    if (*iter && iter->IsReadyToDraw()) {
      const ManagedTileState::DrawInfo& draw_info = iter->draw_info();
      switch (draw_info.mode()) {
        case ManagedTileState::DrawInfo::RESOURCE_MODE: {
          gfx::RectF texture_rect = iter.texture_rect();

          // The raster_contents_scale_ is the best scale that the layer is
          // trying to produce, even though it may not be ideal. Since that's
          // the best the layer can promise in the future, consider those as
          // complete. But if a tile is ideal scale, we don't want to consider
          // it incomplete and trying to replace it with a tile at a worse
          // scale.
          if (iter->contents_scale() != raster_contents_scale_ &&
              iter->contents_scale() != ideal_contents_scale_ &&
              geometry_rect.Intersects(scaled_viewport_for_tile_priority)) {
            append_quads_data->num_incomplete_tiles++;
          }

          TileDrawQuad* quad =
              render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
          quad->SetNew(shared_quad_state,
                       geometry_rect,
                       opaque_rect,
                       visible_geometry_rect,
                       draw_info.get_resource_id(),
                       texture_rect,
                       iter.texture_size(),
                       draw_info.contents_swizzled());
          has_draw_quad = true;
          break;
        }
        case ManagedTileState::DrawInfo::PICTURE_PILE_MODE: {
          if (!layer_tree_impl()
                   ->GetRendererCapabilities()
                   .allow_rasterize_on_demand) {
            ++on_demand_missing_tile_count;
            break;
          }

          gfx::RectF texture_rect = iter.texture_rect();

          ResourceProvider* resource_provider =
              layer_tree_impl()->resource_provider();
          ResourceFormat format =
              resource_provider->memory_efficient_texture_format();
          PictureDrawQuad* quad =
              render_pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
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
          has_draw_quad = true;
          break;
        }
        case ManagedTileState::DrawInfo::SOLID_COLOR_MODE: {
          SolidColorDrawQuad* quad =
              render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
          quad->SetNew(shared_quad_state,
                       geometry_rect,
                       visible_geometry_rect,
                       draw_info.get_solid_color(),
                       false);
          has_draw_quad = true;
          break;
        }
      }
    }

    if (!has_draw_quad) {
      if (draw_checkerboard_for_missing_tiles()) {
        CheckerboardDrawQuad* quad =
            render_pass->CreateAndAppendDrawQuad<CheckerboardDrawQuad>();
        SkColor color = DebugColors::DefaultCheckerboardColor();
        quad->SetNew(
            shared_quad_state, geometry_rect, visible_geometry_rect, color);
      } else {
        SkColor color = SafeOpaqueBackgroundColor();
        SolidColorDrawQuad* quad =
            render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     visible_geometry_rect,
                     color,
                     false);
      }

      if (geometry_rect.Intersects(scaled_viewport_for_tile_priority)) {
        append_quads_data->num_missing_tiles++;
        ++missing_tile_count;
      }
      append_quads_data->approximated_visible_content_area +=
          visible_geometry_rect.width() * visible_geometry_rect.height();
      continue;
    }

    if (iter.resolution() != HIGH_RESOLUTION) {
      append_quads_data->approximated_visible_content_area +=
          visible_geometry_rect.width() * visible_geometry_rect.height();
    }

    // If we have a draw quad, but it's not low resolution, then
    // mark that we've used something other than low res to draw.
    if (iter.resolution() != LOW_RESOLUTION)
      only_used_low_res_last_append_quads_ = false;

    if (seen_tilings.empty() || seen_tilings.back() != iter.CurrentTiling())
      seen_tilings.push_back(iter.CurrentTiling());
  }

  if (missing_tile_count) {
    TRACE_EVENT_INSTANT2("cc",
                         "PictureLayerImpl::AppendQuads checkerboard",
                         TRACE_EVENT_SCOPE_THREAD,
                         "missing_tile_count",
                         missing_tile_count,
                         "on_demand_missing_tile_count",
                         on_demand_missing_tile_count);
  }

  // Aggressively remove any tilings that are not seen to save memory. Note
  // that this is at the expense of doing cause more frequent re-painting. A
  // better scheme would be to maintain a tighter visible_content_rect for the
  // finer tilings.
  CleanUpTilingsOnActiveLayer(seen_tilings);
}

void PictureLayerImpl::UpdateTiles(const Occlusion& occlusion_in_content_space,
                                   bool resourceless_software_draw) {
  TRACE_EVENT0("cc", "PictureLayerImpl::UpdateTiles");
  DCHECK_EQ(1.f, contents_scale_x());
  DCHECK_EQ(1.f, contents_scale_y());

  DoPostCommitInitializationIfNeeded();

  if (!resourceless_software_draw) {
    visible_rect_for_tile_priority_ = visible_content_rect();
  }

  if (!CanHaveTilings()) {
    ideal_page_scale_ = 0.f;
    ideal_device_scale_ = 0.f;
    ideal_contents_scale_ = 0.f;
    ideal_source_scale_ = 0.f;
    SanityCheckTilingState();
    return;
  }

  UpdateIdealScales();

  DCHECK(tilings_->num_tilings() > 0 || raster_contents_scale_ == 0.f)
      << "A layer with no tilings shouldn't have valid raster scales";
  if (!raster_contents_scale_ || ShouldAdjustRasterScale()) {
    RecalculateRasterScales();
    AddTilingsForRasterScale();
  }

  DCHECK(raster_page_scale_);
  DCHECK(raster_device_scale_);
  DCHECK(raster_source_scale_);
  DCHECK(raster_contents_scale_);
  DCHECK(low_res_raster_contents_scale_);

  was_screen_space_transform_animating_ =
      draw_properties().screen_space_transform_is_animating;

  if (draw_transform_is_animating())
    pile_->set_likely_to_be_used_for_transform_animation();

  should_update_tile_priorities_ = true;

  UpdateTilePriorities(occlusion_in_content_space);
}

void PictureLayerImpl::UpdateTilePriorities(
    const Occlusion& occlusion_in_content_space) {
  DCHECK(!pile_->is_solid_color() || !tilings_->num_tilings());

  TRACE_EVENT0("cc", "PictureLayerImpl::UpdateTilePriorities");

  double current_frame_time_in_seconds =
      (layer_tree_impl()->CurrentBeginFrameArgs().frame_time -
       base::TimeTicks()).InSecondsF();

  gfx::Rect viewport_rect_in_layer_space =
      GetViewportForTilePriorityInContentSpace();
  bool tiling_needs_update = false;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    if (tilings_->tiling_at(i)->NeedsUpdateForFrameAtTimeAndViewport(
            current_frame_time_in_seconds, viewport_rect_in_layer_space)) {
      tiling_needs_update = true;
      break;
    }
  }
  if (!tiling_needs_update)
    return;

  WhichTree tree =
      layer_tree_impl()->IsActiveTree() ? ACTIVE_TREE : PENDING_TREE;
  bool can_require_tiles_for_activation =
      !only_used_low_res_last_append_quads_ || RequiresHighResToDraw() ||
      !layer_tree_impl()->SmoothnessTakesPriority();
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);

    tiling->set_can_require_tiles_for_activation(
        can_require_tiles_for_activation);

    // Pass |occlusion_in_content_space| for |occlusion_in_layer_space| since
    // they are the same space in picture layer, as contents scale is always 1.
    tiling->ComputeTilePriorityRects(tree,
                                     viewport_rect_in_layer_space,
                                     ideal_contents_scale_,
                                     current_frame_time_in_seconds,
                                     occlusion_in_content_space);
  }

  // Tile priorities were modified.
  // TODO(vmpstr): See if this can be removed in favour of calling it from LTHI
  layer_tree_impl()->DidModifyTilePriorities();
}

gfx::Rect PictureLayerImpl::GetViewportForTilePriorityInContentSpace() const {
  // If visible_rect_for_tile_priority_ is empty or
  // viewport_rect_for_tile_priority is set to be different from the device
  // viewport, try to inverse project the viewport into layer space and use
  // that. Otherwise just use visible_rect_for_tile_priority_
  gfx::Rect visible_rect_in_content_space = visible_rect_for_tile_priority_;
  gfx::Rect viewport_rect_for_tile_priority =
      layer_tree_impl()->ViewportRectForTilePriority();

  if (visible_rect_in_content_space.IsEmpty() ||
      layer_tree_impl()->DeviceViewport() != viewport_rect_for_tile_priority) {
    gfx::Transform view_to_layer(gfx::Transform::kSkipInitialization);
    if (screen_space_transform().GetInverse(&view_to_layer)) {
      // Transform from view space to content space.
      visible_rect_in_content_space =
          gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
              view_to_layer, viewport_rect_for_tile_priority));
    }
  }
  return visible_rect_in_content_space;
}

PictureLayerImpl* PictureLayerImpl::GetPendingOrActiveTwinLayer() const {
  if (!twin_layer_ || !twin_layer_->IsOnActiveOrPendingTree())
    return nullptr;
  return twin_layer_;
}

PictureLayerImpl* PictureLayerImpl::GetRecycledTwinLayer() const {
  if (!twin_layer_ || twin_layer_->IsOnActiveOrPendingTree())
    return nullptr;
  return twin_layer_;
}

void PictureLayerImpl::NotifyTileStateChanged(const Tile* tile) {
  if (layer_tree_impl()->IsActiveTree()) {
    gfx::RectF layer_damage_rect =
        gfx::ScaleRect(tile->content_rect(), 1.f / tile->contents_scale());
    AddDamageRect(layer_damage_rect);
  }
}

void PictureLayerImpl::DidBecomeActive() {
  LayerImpl::DidBecomeActive();
  // TODO(vmpstr): See if this can be removed in favour of calling it from LTHI
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

skia::RefPtr<SkPicture> PictureLayerImpl::GetPicture() {
  return pile_->GetFlattenedPicture();
}

scoped_refptr<Tile> PictureLayerImpl::CreateTile(PictureLayerTiling* tiling,
                                               const gfx::Rect& content_rect) {
  DCHECK(!pile_->is_solid_color());
  if (!pile_->CanRaster(tiling->contents_scale(), content_rect))
    return scoped_refptr<Tile>();

  int flags = 0;

  // TODO(vmpstr): Revisit this. For now, enabling analysis means that we get as
  // much savings on memory as we can. However, for some cases like ganesh or
  // small layers, the amount of time we spend analyzing might not justify
  // memory savings that we can get. Note that we don't handle solid color
  // masks, so we shouldn't bother analyzing those.
  // Bugs: crbug.com/397198, crbug.com/396908
  if (!pile_->is_mask())
    flags = Tile::USE_PICTURE_ANALYSIS;

  return layer_tree_impl()->tile_manager()->CreateTile(
      pile_.get(),
      content_rect.size(),
      content_rect,
      tiling->contents_scale(),
      id(),
      layer_tree_impl()->source_frame_number(),
      flags);
}

RasterSource* PictureLayerImpl::GetRasterSource() {
  return pile_.get();
}

const Region* PictureLayerImpl::GetPendingInvalidation() {
  if (layer_tree_impl()->IsPendingTree())
    return &invalidation_;
  DCHECK(layer_tree_impl()->IsActiveTree());
  if (PictureLayerImpl* twin_layer = GetPendingOrActiveTwinLayer())
    return &twin_layer->invalidation_;
  return nullptr;
}

const PictureLayerTiling* PictureLayerImpl::GetPendingOrActiveTwinTiling(
    const PictureLayerTiling* tiling) const {
  PictureLayerImpl* twin_layer = GetPendingOrActiveTwinLayer();
  if (!twin_layer)
    return nullptr;
  // TODO(danakj): Remove this when no longer swapping tilings.
  if (!twin_layer->tilings_)
    return nullptr;
  return twin_layer->tilings_->TilingAtScale(tiling->contents_scale());
}

PictureLayerTiling* PictureLayerImpl::GetRecycledTwinTiling(
    const PictureLayerTiling* tiling) {
  PictureLayerImpl* recycled_twin = GetRecycledTwinLayer();
  if (!recycled_twin || !recycled_twin->tilings_)
    return nullptr;
  return recycled_twin->tilings_->TilingAtScale(tiling->contents_scale());
}

size_t PictureLayerImpl::GetMaxTilesForInterestArea() const {
  return layer_tree_impl()->settings().max_tiles_for_interest_area;
}

float PictureLayerImpl::GetSkewportTargetTimeInSeconds() const {
  float skewport_target_time_in_frames =
      layer_tree_impl()->use_gpu_rasterization()
          ? kGpuSkewportTargetTimeInFrames
          : kCpuSkewportTargetTimeInFrames;
  return skewport_target_time_in_frames *
         layer_tree_impl()->begin_impl_frame_interval().InSecondsF() *
         layer_tree_impl()->settings().skewport_target_time_multiplier;
}

int PictureLayerImpl::GetSkewportExtrapolationLimitInContentPixels() const {
  return layer_tree_impl()
      ->settings()
      .skewport_extrapolation_limit_in_content_pixels;
}

bool PictureLayerImpl::RequiresHighResToDraw() const {
  return layer_tree_impl()->RequiresHighResToDraw();
}

gfx::Size PictureLayerImpl::CalculateTileSize(
    const gfx::Size& content_bounds) const {
  int max_texture_size =
      layer_tree_impl()->resource_provider()->max_texture_size();

  if (pile_->is_mask()) {
    // Masks are not tiled, so if we can't cover the whole mask with one tile,
    // don't make any tiles at all. Returning an empty size signals this.
    if (content_bounds.width() > max_texture_size ||
        content_bounds.height() > max_texture_size)
      return gfx::Size();
    return content_bounds;
  }

  int default_tile_width = 0;
  int default_tile_height = 0;
  if (layer_tree_impl()->use_gpu_rasterization()) {
    // For GPU rasterization, we pick an ideal tile size using the viewport
    // so we don't need any settings. The current approach uses 4 tiles
    // to cover the viewport vertically.
    int viewport_width = layer_tree_impl()->device_viewport_size().width();
    int viewport_height = layer_tree_impl()->device_viewport_size().height();
    default_tile_width = viewport_width;
    // Also, increase the height proportionally as the width decreases, and
    // pad by our border texels to make the tiles exactly match the viewport.
    int divisor = 4;
    if (content_bounds.width() <= viewport_width / 2)
      divisor = 2;
    if (content_bounds.width() <= viewport_width / 4)
      divisor = 1;
    default_tile_height = RoundUp(viewport_height, divisor) / divisor;
    default_tile_height += 2 * PictureLayerTiling::kBorderTexels;
    default_tile_height =
        std::max(default_tile_height, kMinHeightForGpuRasteredTile);
  } else {
    // For CPU rasterization we use tile-size settings.
    const LayerTreeSettings& settings = layer_tree_impl()->settings();
    int max_untiled_content_width = settings.max_untiled_layer_size.width();
    int max_untiled_content_height = settings.max_untiled_layer_size.height();
    default_tile_width = settings.default_tile_size.width();
    default_tile_height = settings.default_tile_size.height();

    // If the content width is small, increase tile size vertically.
    // If the content height is small, increase tile size horizontally.
    // If both are less than the untiled-size, use a single tile.
    if (content_bounds.width() < default_tile_width)
      default_tile_height = max_untiled_content_height;
    if (content_bounds.height() < default_tile_height)
      default_tile_width = max_untiled_content_width;
    if (content_bounds.width() < max_untiled_content_width &&
        content_bounds.height() < max_untiled_content_height) {
      default_tile_height = max_untiled_content_height;
      default_tile_width = max_untiled_content_width;
    }
  }

  int tile_width = default_tile_width;
  int tile_height = default_tile_height;

  // Clamp the tile width/height to the content width/height to save space.
  if (content_bounds.width() < default_tile_width) {
    tile_width = std::min(tile_width, content_bounds.width());
    tile_width = RoundUp(tile_width, kTileRoundUp);
    tile_width = std::min(tile_width, default_tile_width);
  }
  if (content_bounds.height() < default_tile_height) {
    tile_height = std::min(tile_height, content_bounds.height());
    tile_height = RoundUp(tile_height, kTileRoundUp);
    tile_height = std::min(tile_height, default_tile_height);
  }

  // Under no circumstance should we be larger than the max texture size.
  tile_width = std::min(tile_width, max_texture_size);
  tile_height = std::min(tile_height, max_texture_size);
  return gfx::Size(tile_width, tile_height);
}

void PictureLayerImpl::SyncFromActiveLayer(const PictureLayerImpl* other) {
  TRACE_EVENT0("cc", "SyncFromActiveLayer");
  DCHECK(!other->needs_post_commit_initialization_);
  DCHECK(other->tilings_);

  if (!DrawsContent()) {
    RemoveAllTilings();
    return;
  }

  raster_page_scale_ = other->raster_page_scale_;
  raster_device_scale_ = other->raster_device_scale_;
  raster_source_scale_ = other->raster_source_scale_;
  raster_contents_scale_ = other->raster_contents_scale_;
  low_res_raster_contents_scale_ = other->low_res_raster_contents_scale_;

  bool synced_high_res_tiling = false;
  if (CanHaveTilings()) {
    synced_high_res_tiling = tilings_->SyncTilings(*other->tilings_,
                                                   pile_->tiling_size(),
                                                   invalidation_,
                                                   MinimumContentsScale());
  } else {
    RemoveAllTilings();
  }

  // If our MinimumContentsScale has changed to prevent the twin's high res
  // tiling from being synced, we should reset the raster scale and let it be
  // recalculated (1) again. This can happen if our bounds shrink to the point
  // where min contents scale grows.
  // (1) - TODO(vmpstr) Instead of hoping that this will be recalculated, we
  // should refactor this code a little bit and actually recalculate this.
  // However, this is a larger undertaking, so this will work for now.
  if (!synced_high_res_tiling)
    ResetRasterScale();
  else
    SanityCheckTilingState();
}

void PictureLayerImpl::SyncTiling(
    const PictureLayerTiling* tiling) {
  if (!tilings_)
    return;
  if (!CanHaveTilingWithScale(tiling->contents_scale()))
    return;
  tilings_->AddTiling(tiling->contents_scale(), pile_->tiling_size());

  // If this tree needs update draw properties, then the tiling will
  // get updated prior to drawing or activation.  If this tree does not
  // need update draw properties, then its transforms are up to date and
  // we can create tiles for this tiling immediately.
  if (!layer_tree_impl()->needs_update_draw_properties() &&
      should_update_tile_priorities_) {
    // TODO(danakj): Add a DCHECK() that we are not using occlusion tracking
    // when we stop using the pending tree in the browser compositor. If we want
    // to support occlusion tracking here, we need to dirty the draw properties
    // or save occlusion as a draw property.
    UpdateTilePriorities(Occlusion());
  }
}

void PictureLayerImpl::GetContentsResourceId(
    ResourceProvider::ResourceId* resource_id,
    gfx::Size* resource_size) const {
  DCHECK_EQ(bounds().ToString(), pile_->tiling_size().ToString());
  gfx::Rect content_rect(bounds());
  PictureLayerTilingSet::CoverageIterator iter(
      tilings_.get(), 1.f, content_rect, ideal_contents_scale_);

  // Mask resource not ready yet.
  if (!iter || !*iter) {
    *resource_id = 0;
    return;
  }

  // Masks only supported if they fit on exactly one tile.
  DCHECK(iter.geometry_rect() == content_rect)
      << "iter rect " << iter.geometry_rect().ToString() << " content rect "
      << content_rect.ToString();

  const ManagedTileState::DrawInfo& draw_info = iter->draw_info();
  if (!draw_info.IsReadyToDraw() ||
      draw_info.mode() != ManagedTileState::DrawInfo::RESOURCE_MODE) {
    *resource_id = 0;
    return;
  }

  *resource_id = draw_info.get_resource_id();
  *resource_size = iter.texture_size();
}

void PictureLayerImpl::DoPostCommitInitialization() {
  DCHECK(needs_post_commit_initialization_);
  DCHECK(layer_tree_impl()->IsPendingTree());

  if (!tilings_)
    tilings_ = make_scoped_ptr(new PictureLayerTilingSet(this));

  PictureLayerImpl* twin_layer = GetPendingOrActiveTwinLayer();
  if (twin_layer) {
    // If the twin has never been pushed to, do not sync from it.
    // This can happen if this function is called during activation.
    if (!twin_layer->needs_post_commit_initialization_)
      SyncFromActiveLayer(twin_layer);
  }

  needs_post_commit_initialization_ = false;
}

PictureLayerTiling* PictureLayerImpl::AddTiling(float contents_scale) {
  DCHECK(CanHaveTilingWithScale(contents_scale)) <<
      "contents_scale: " << contents_scale;

  PictureLayerTiling* tiling =
      tilings_->AddTiling(contents_scale, pile_->tiling_size());

  DCHECK(pile_->HasRecordings());

  if (PictureLayerImpl* twin_layer = GetPendingOrActiveTwinLayer())
    twin_layer->SyncTiling(tiling);

  return tiling;
}

void PictureLayerImpl::RemoveTiling(float contents_scale) {
  if (!tilings_ || tilings_->num_tilings() == 0)
    return;

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

void PictureLayerImpl::AddTilingsForRasterScale() {
  PictureLayerTiling* high_res = nullptr;
  PictureLayerTiling* low_res = nullptr;

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

  // Only create new low res tilings when the transform is static.  This
  // prevents wastefully creating a paired low res tiling for every new high res
  // tiling during a pinch or a CSS animation.
  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (layer_tree_impl()->create_low_res_tiling() && !is_pinching &&
      !draw_properties().screen_space_transform_is_animating && !low_res &&
      low_res != high_res)
    low_res = AddTiling(low_res_raster_contents_scale_);

  // Set low-res if we have one.
  if (low_res && low_res != high_res)
    low_res->set_resolution(LOW_RESOLUTION);

  // Make sure we always have one high-res (even if high == low).
  high_res->set_resolution(HIGH_RESOLUTION);

  SanityCheckTilingState();
}

bool PictureLayerImpl::ShouldAdjustRasterScale() const {
  if (was_screen_space_transform_animating_ !=
      draw_properties().screen_space_transform_is_animating)
    return true;

  if (draw_properties().screen_space_transform_is_animating &&
      raster_contents_scale_ != ideal_contents_scale_ &&
      ShouldAdjustRasterScaleDuringScaleAnimations())
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

  // When the source scale changes we want to match it, but not when animating
  // or when we've fixed the scale in place.
  if (!draw_properties().screen_space_transform_is_animating &&
      !raster_source_scale_is_fixed_ &&
      raster_source_scale_ != ideal_source_scale_)
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

void PictureLayerImpl::RecalculateRasterScales() {
  float old_raster_contents_scale = raster_contents_scale_;
  float old_raster_page_scale = raster_page_scale_;
  float old_raster_source_scale = raster_source_scale_;

  raster_device_scale_ = ideal_device_scale_;
  raster_page_scale_ = ideal_page_scale_;
  raster_source_scale_ = ideal_source_scale_;
  raster_contents_scale_ = ideal_contents_scale_;

  // If we're not animating, or leaving an animation, and the
  // ideal_source_scale_ changes, then things are unpredictable, and we fix
  // the raster_source_scale_ in place.
  if (old_raster_source_scale &&
      !draw_properties().screen_space_transform_is_animating &&
      !was_screen_space_transform_animating_ &&
      old_raster_source_scale != ideal_source_scale_)
    raster_source_scale_is_fixed_ = true;

  // TODO(danakj): Adjust raster source scale closer to ideal source scale at
  // a throttled rate. Possibly make use of invalidation_.IsEmpty() on pending
  // tree. This will allow CSS scale changes to get re-rastered at an
  // appropriate rate. (crbug.com/413636)
  if (raster_source_scale_is_fixed_) {
    raster_contents_scale_ /= raster_source_scale_;
    raster_source_scale_ = 1.f;
  }

  // During pinch we completely ignore the current ideal scale, and just use
  // a multiple of the previous scale.
  // TODO(danakj): This seems crazy, we should use the current ideal, no?
  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (is_pinching && old_raster_contents_scale) {
    // See ShouldAdjustRasterScale:
    // - When zooming out, preemptively create new tiling at lower resolution.
    // - When zooming in, approximate ideal using multiple of kMaxScaleRatio.
    bool zooming_out = old_raster_page_scale > ideal_page_scale_;
    float desired_contents_scale =
        zooming_out ? old_raster_contents_scale / kMaxScaleRatioDuringPinch
                    : old_raster_contents_scale * kMaxScaleRatioDuringPinch;
    raster_contents_scale_ = SnappedContentsScale(desired_contents_scale);
    raster_page_scale_ =
        raster_contents_scale_ / raster_device_scale_ / raster_source_scale_;
  }

  raster_contents_scale_ =
      std::max(raster_contents_scale_, MinimumContentsScale());

  // If we're not re-rasterizing during animation, rasterize at the maximum
  // scale that will occur during the animation, if the maximum scale is
  // known. However we want to avoid excessive memory use. If the scale is
  // smaller than what we would choose otherwise, then it's always better off
  // for us memory-wise. But otherwise, we don't choose a scale at which this
  // layer's rastered content would become larger than the viewport.
  if (draw_properties().screen_space_transform_is_animating &&
      !ShouldAdjustRasterScaleDuringScaleAnimations()) {
    bool can_raster_at_maximum_scale = false;
    // TODO(ajuma): If we need to deal with scale-down animations starting right
    // as a layer gets promoted, then we'd want to have the
    // |starting_animation_contents_scale| passed in here as a separate draw
    // property so we could try use that when the max is too large.
    // See crbug.com/422341.
    float maximum_scale = draw_properties().maximum_animation_contents_scale;
    if (maximum_scale) {
      gfx::Size bounds_at_maximum_scale = gfx::ToCeiledSize(
          gfx::ScaleSize(pile_->tiling_size(), maximum_scale));
      if (bounds_at_maximum_scale.GetArea() <=
          layer_tree_impl()->device_viewport_size().GetArea())
        can_raster_at_maximum_scale = true;
    }
    // Use the computed scales for the raster scale directly, do not try to use
    // the ideal scale here. The current ideal scale may be way too large in the
    // case of an animation with scale, and will be constantly changing.
    if (can_raster_at_maximum_scale)
      raster_contents_scale_ = maximum_scale;
    else
      raster_contents_scale_ = 1.f * ideal_page_scale_ * ideal_device_scale_;
  }

  // If this layer would create zero or one tiles at this content scale,
  // don't create a low res tiling.
  gfx::Size raster_bounds = gfx::ToCeiledSize(
      gfx::ScaleSize(pile_->tiling_size(), raster_contents_scale_));
  gfx::Size tile_size = CalculateTileSize(raster_bounds);
  bool tile_covers_bounds = tile_size.width() >= raster_bounds.width() &&
                            tile_size.height() >= raster_bounds.height();
  if (tile_size.IsEmpty() || tile_covers_bounds) {
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
  float twin_low_res_scale = 0.f;

  PictureLayerImpl* twin = GetPendingOrActiveTwinLayer();
  if (twin && twin->CanHaveTilings()) {
    min_acceptable_high_res_scale = std::min(
        min_acceptable_high_res_scale,
        std::min(twin->raster_contents_scale_, twin->ideal_contents_scale_));
    max_acceptable_high_res_scale = std::max(
        max_acceptable_high_res_scale,
        std::max(twin->raster_contents_scale_, twin->ideal_contents_scale_));

    // TODO(danakj): Remove the tilings_ check when we create them in the
    // constructor.
    if (twin->tilings_) {
      for (size_t i = 0; i < twin->tilings_->num_tilings(); ++i) {
        PictureLayerTiling* tiling = twin->tilings_->tiling_at(i);
        if (tiling->resolution() == LOW_RESOLUTION)
          twin_low_res_scale = tiling->contents_scale();
      }
    }
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
    if (layer_tree_impl()->create_low_res_tiling()) {
      if (tiling->resolution() == LOW_RESOLUTION ||
          tiling->contents_scale() == twin_low_res_scale)
        continue;
    }

    // Don't remove tilings that are being used (and thus would cause a flash.)
    if (std::find(used_tilings.begin(), used_tilings.end(), tiling) !=
        used_tilings.end())
      continue;

    to_remove.push_back(tiling);
  }

  if (to_remove.empty())
    return;

  PictureLayerImpl* recycled_twin = GetRecycledTwinLayer();
  // Remove tilings on this tree and the twin tree.
  for (size_t i = 0; i < to_remove.size(); ++i) {
    const PictureLayerTiling* twin_tiling =
        GetPendingOrActiveTwinTiling(to_remove[i]);
    // Only remove tilings from the twin layer if they have
    // NON_IDEAL_RESOLUTION.
    if (twin_tiling && twin_tiling->resolution() == NON_IDEAL_RESOLUTION)
      twin->RemoveTiling(to_remove[i]->contents_scale());
    // Remove the tiling from the recycle tree. Note that we ignore resolution,
    // since we don't need to maintain high/low res on the recycle tree.
    if (recycled_twin)
      recycled_twin->RemoveTiling(to_remove[i]->contents_scale());
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
  int min_dimension =
      std::min(pile_->tiling_size().width(), pile_->tiling_size().height());
  if (!min_dimension)
    return setting_min;

  return std::max(1.f / min_dimension, setting_min);
}

void PictureLayerImpl::ResetRasterScale() {
  raster_page_scale_ = 0.f;
  raster_device_scale_ = 0.f;
  raster_source_scale_ = 0.f;
  raster_contents_scale_ = 0.f;
  low_res_raster_contents_scale_ = 0.f;
  raster_source_scale_is_fixed_ = false;

  // When raster scales aren't valid, don't update tile priorities until
  // this layer has been updated via UpdateDrawProperties.
  should_update_tile_priorities_ = false;
}

bool PictureLayerImpl::CanHaveTilings() const {
  if (pile_->is_solid_color())
    return false;
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
  // Recycle tree doesn't have any restrictions.
  if (layer_tree_impl()->IsRecycleTree())
    return;

  if (!CanHaveTilings()) {
    DCHECK_EQ(0u, tilings_->num_tilings());
    return;
  }
  if (tilings_->num_tilings() == 0)
    return;

  // We should only have one high res tiling.
  DCHECK_EQ(1, tilings_->NumHighResTilings());
#endif
}

bool PictureLayerImpl::ShouldAdjustRasterScaleDuringScaleAnimations() const {
  return layer_tree_impl()->use_gpu_rasterization();
}

float PictureLayerImpl::MaximumTilingContentsScale() const {
  float max_contents_scale = MinimumContentsScale();
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings_->tiling_at(i);
    max_contents_scale = std::max(max_contents_scale, tiling->contents_scale());
  }
  return max_contents_scale;
}

void PictureLayerImpl::UpdateIdealScales() {
  DCHECK(CanHaveTilings());

  float min_contents_scale = MinimumContentsScale();
  DCHECK_GT(min_contents_scale, 0.f);
  float min_page_scale = layer_tree_impl()->min_page_scale_factor();
  DCHECK_GT(min_page_scale, 0.f);
  float min_device_scale = 1.f;
  float min_source_scale =
      min_contents_scale / min_page_scale / min_device_scale;

  float ideal_page_scale = draw_properties().page_scale_factor;
  float ideal_device_scale = draw_properties().device_scale_factor;
  float ideal_source_scale = draw_properties().ideal_contents_scale /
                             ideal_page_scale / ideal_device_scale;
  ideal_contents_scale_ =
      std::max(draw_properties().ideal_contents_scale, min_contents_scale);
  ideal_page_scale_ = draw_properties().page_scale_factor;
  ideal_device_scale_ = draw_properties().device_scale_factor;
  ideal_source_scale_ = std::max(ideal_source_scale, min_source_scale);
}

void PictureLayerImpl::GetDebugBorderProperties(
    SkColor* color,
    float* width) const {
  *color = DebugColors::TiledContentLayerBorderColor();
  *width = DebugColors::TiledContentLayerBorderWidth(layer_tree_impl());
}

void PictureLayerImpl::GetAllTilesForTracing(
    std::set<const Tile*>* tiles) const {
  if (!tilings_)
    return;

  for (size_t i = 0; i < tilings_->num_tilings(); ++i)
    tilings_->tiling_at(i)->GetAllTilesForTracing(tiles);
}

void PictureLayerImpl::AsValueInto(base::debug::TracedValue* state) const {
  const_cast<PictureLayerImpl*>(this)->DoPostCommitInitializationIfNeeded();
  LayerImpl::AsValueInto(state);
  state->SetDouble("ideal_contents_scale", ideal_contents_scale_);
  state->SetDouble("geometry_contents_scale", MaximumTilingContentsScale());
  state->BeginArray("tilings");
  tilings_->AsValueInto(state);
  state->EndArray();

  state->BeginArray("tile_priority_rect");
  MathUtil::AddToTracedValue(GetViewportForTilePriorityInContentSpace(), state);
  state->EndArray();

  state->BeginArray("visible_rect");
  MathUtil::AddToTracedValue(visible_content_rect(), state);
  state->EndArray();

  state->BeginArray("pictures");
  pile_->AsValueInto(state);
  state->EndArray();

  state->BeginArray("invalidation");
  invalidation_.AsValueInto(state);
  state->EndArray();

  state->BeginArray("coverage_tiles");
  for (PictureLayerTilingSet::CoverageIterator iter(
           tilings_.get(),
           1.f,
           gfx::Rect(pile_->tiling_size()),
           ideal_contents_scale_);
       iter;
       ++iter) {
    state->BeginDictionary();

    state->BeginArray("geometry_rect");
    MathUtil::AddToTracedValue(iter.geometry_rect(), state);
    state->EndArray();

    if (*iter)
      TracedValue::SetIDRef(*iter, state, "tile");

    state->EndDictionary();
  }
  state->EndArray();
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

bool PictureLayerImpl::HasValidTilePriorities() const {
  return IsOnActiveOrPendingTree() && IsDrawnRenderSurfaceLayerListMember();
}

bool PictureLayerImpl::AllTilesRequiredForActivationAreReadyToDraw() const {
  TRACE_EVENT0("cc",
               "PictureLayerImpl::AllTilesRequiredForActivationAreReadyToDraw");
  if (!layer_tree_impl()->IsPendingTree())
    return true;

  if (!HasValidTilePriorities())
    return true;

  if (!tilings_)
    return true;

  if (visible_rect_for_tile_priority_.IsEmpty())
    return true;

  gfx::Rect rect = GetViewportForTilePriorityInContentSpace();
  rect.Intersect(visible_rect_for_tile_priority_);

  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (tiling->resolution() != HIGH_RESOLUTION &&
        tiling->resolution() != LOW_RESOLUTION)
      continue;

    for (PictureLayerTiling::CoverageIterator iter(tiling, 1.f, rect); iter;
         ++iter) {
      const Tile* tile = *iter;
      // A null tile (i.e. missing recording) can just be skipped.
      // TODO(vmpstr): Verify this is true if we create tiles in raster
      // iterators.
      if (!tile)
        continue;

      // We can't check tile->required_for_activation, because that value might
      // be out of date. It is updated in the raster/eviction iterators.
      // TODO(vmpstr): Remove the comment once you can't access this information
      // from the tile.
      if (tiling->IsTileRequiredForActivation(tile) && !tile->IsReadyToDraw()) {
        TRACE_EVENT_INSTANT0("cc",
                             "PictureLayerImpl::"
                             "AllTilesRequiredForActivationAreReadyToDraw not "
                             "ready to activate",
                             TRACE_EVENT_SCOPE_THREAD);
        return false;
      }
    }
  }

  return true;
}

PictureLayerImpl::LayerRasterTileIterator::LayerRasterTileIterator()
    : layer_(nullptr), current_stage_(arraysize(stages_)) {
}

PictureLayerImpl::LayerRasterTileIterator::LayerRasterTileIterator(
    PictureLayerImpl* layer,
    bool prioritize_low_res)
    : layer_(layer), current_stage_(0) {
  DCHECK(layer_);

  // Early out if the layer has no tilings.
  if (!layer_->tilings_ || !layer_->tilings_->num_tilings()) {
    current_stage_ = arraysize(stages_);
    return;
  }

  // Tiles without valid priority are treated as having lowest priority and
  // never considered for raster.
  if (!layer_->HasValidTilePriorities()) {
    current_stage_ = arraysize(stages_);
    return;
  }

  // Find high and low res tilings and initialize the iterators.
  for (size_t i = 0; i < layer_->tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = layer_->tilings_->tiling_at(i);
    if (tiling->resolution() == HIGH_RESOLUTION) {
      iterators_[HIGH_RES] =
          PictureLayerTiling::TilingRasterTileIterator(tiling);
    }

    if (prioritize_low_res && tiling->resolution() == LOW_RESOLUTION) {
      iterators_[LOW_RES] =
          PictureLayerTiling::TilingRasterTileIterator(tiling);
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
    AdvanceToNextStage();
}

PictureLayerImpl::LayerRasterTileIterator::~LayerRasterTileIterator() {}

PictureLayerImpl::LayerRasterTileIterator::operator bool() const {
  return current_stage_ < arraysize(stages_);
}

PictureLayerImpl::LayerRasterTileIterator&
PictureLayerImpl::LayerRasterTileIterator::
operator++() {
  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

  // First advance the iterator.
  DCHECK(iterators_[index]);
  DCHECK(iterators_[index].get_type() == tile_type);
  ++iterators_[index];

  if (!iterators_[index] || iterators_[index].get_type() != tile_type)
    AdvanceToNextStage();

  return *this;
}

Tile* PictureLayerImpl::LayerRasterTileIterator::operator*() {
  DCHECK(*this);

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(iterators_[index]);
  DCHECK(iterators_[index].get_type() == stages_[current_stage_].tile_type);

  return *iterators_[index];
}

const Tile* PictureLayerImpl::LayerRasterTileIterator::operator*() const {
  DCHECK(*this);

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(iterators_[index]);
  DCHECK(iterators_[index].get_type() == stages_[current_stage_].tile_type);

  return *iterators_[index];
}

void PictureLayerImpl::LayerRasterTileIterator::AdvanceToNextStage() {
  DCHECK_LT(current_stage_, arraysize(stages_));
  ++current_stage_;
  while (current_stage_ < arraysize(stages_)) {
    IteratorType index = stages_[current_stage_].iterator_type;
    TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

    if (iterators_[index] && iterators_[index].get_type() == tile_type)
      break;
    ++current_stage_;
  }
}

PictureLayerImpl::LayerEvictionTileIterator::LayerEvictionTileIterator()
    : layer_(nullptr),
      tree_priority_(SAME_PRIORITY_FOR_BOTH_TREES),
      current_category_(PictureLayerTiling::EVENTUALLY),
      current_tiling_range_type_(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES),
      current_tiling_(0u) {
}

PictureLayerImpl::LayerEvictionTileIterator::LayerEvictionTileIterator(
    PictureLayerImpl* layer,
    TreePriority tree_priority)
    : layer_(layer),
      tree_priority_(tree_priority),
      current_category_(PictureLayerTiling::EVENTUALLY),
      current_tiling_range_type_(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES),
      current_tiling_(CurrentTilingRange().start - 1u) {
  // TODO(vmpstr): Once tile priorities are determined by the iterators, ensure
  // that layers that don't have valid tile priorities have lowest priorities so
  // they evict their tiles first (crbug.com/381704)
  DCHECK(layer_->tilings_);
  do {
    if (!AdvanceToNextTiling())
      break;

    current_iterator_ = PictureLayerTiling::TilingEvictionTileIterator(
        layer_->tilings_->tiling_at(CurrentTilingIndex()),
        tree_priority,
        current_category_);
  } while (!current_iterator_);
}

PictureLayerImpl::LayerEvictionTileIterator::~LayerEvictionTileIterator() {
}

Tile* PictureLayerImpl::LayerEvictionTileIterator::operator*() {
  DCHECK(*this);
  return *current_iterator_;
}

const Tile* PictureLayerImpl::LayerEvictionTileIterator::operator*() const {
  DCHECK(*this);
  return *current_iterator_;
}

PictureLayerImpl::LayerEvictionTileIterator&
PictureLayerImpl::LayerEvictionTileIterator::
operator++() {
  DCHECK(*this);
  ++current_iterator_;
  while (!current_iterator_) {
    if (!AdvanceToNextTiling())
      break;

    current_iterator_ = PictureLayerTiling::TilingEvictionTileIterator(
        layer_->tilings_->tiling_at(CurrentTilingIndex()),
        tree_priority_,
        current_category_);
  }
  return *this;
}

PictureLayerImpl::LayerEvictionTileIterator::operator bool() const {
  return !!current_iterator_;
}

bool PictureLayerImpl::LayerEvictionTileIterator::AdvanceToNextCategory() {
  switch (current_category_) {
    case PictureLayerTiling::EVENTUALLY:
      current_category_ =
          PictureLayerTiling::EVENTUALLY_AND_REQUIRED_FOR_ACTIVATION;
      return true;
    case PictureLayerTiling::EVENTUALLY_AND_REQUIRED_FOR_ACTIVATION:
      current_category_ = PictureLayerTiling::SOON;
      return true;
    case PictureLayerTiling::SOON:
      current_category_ = PictureLayerTiling::SOON_AND_REQUIRED_FOR_ACTIVATION;
      return true;
    case PictureLayerTiling::SOON_AND_REQUIRED_FOR_ACTIVATION:
      current_category_ = PictureLayerTiling::NOW;
      return true;
    case PictureLayerTiling::NOW:
      current_category_ = PictureLayerTiling::NOW_AND_REQUIRED_FOR_ACTIVATION;
      return true;
    case PictureLayerTiling::NOW_AND_REQUIRED_FOR_ACTIVATION:
      return false;
  }
  NOTREACHED();
  return false;
}

bool
PictureLayerImpl::LayerEvictionTileIterator::AdvanceToNextTilingRangeType() {
  switch (current_tiling_range_type_) {
    case PictureLayerTilingSet::HIGHER_THAN_HIGH_RES:
      current_tiling_range_type_ = PictureLayerTilingSet::LOWER_THAN_LOW_RES;
      return true;
    case PictureLayerTilingSet::LOWER_THAN_LOW_RES:
      current_tiling_range_type_ =
          PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES;
      return true;
    case PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES:
      current_tiling_range_type_ = PictureLayerTilingSet::LOW_RES;
      return true;
    case PictureLayerTilingSet::LOW_RES:
      current_tiling_range_type_ = PictureLayerTilingSet::HIGH_RES;
      return true;
    case PictureLayerTilingSet::HIGH_RES:
      if (!AdvanceToNextCategory())
        return false;

      current_tiling_range_type_ = PictureLayerTilingSet::HIGHER_THAN_HIGH_RES;
      return true;
  }
  NOTREACHED();
  return false;
}

bool PictureLayerImpl::LayerEvictionTileIterator::AdvanceToNextTiling() {
  DCHECK_NE(current_tiling_, CurrentTilingRange().end);
  ++current_tiling_;
  while (current_tiling_ == CurrentTilingRange().end) {
    if (!AdvanceToNextTilingRangeType())
      return false;

    current_tiling_ = CurrentTilingRange().start;
  }
  return true;
}

PictureLayerTilingSet::TilingRange
PictureLayerImpl::LayerEvictionTileIterator::CurrentTilingRange() const {
  return layer_->tilings_->GetTilingRange(current_tiling_range_type_);
}

size_t PictureLayerImpl::LayerEvictionTileIterator::CurrentTilingIndex() const {
  DCHECK_NE(current_tiling_, CurrentTilingRange().end);
  switch (current_tiling_range_type_) {
    case PictureLayerTilingSet::HIGHER_THAN_HIGH_RES:
    case PictureLayerTilingSet::LOW_RES:
    case PictureLayerTilingSet::HIGH_RES:
      return current_tiling_;
    // Tilings in the following ranges are accessed in reverse order.
    case PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES:
    case PictureLayerTilingSet::LOWER_THAN_LOW_RES: {
      PictureLayerTilingSet::TilingRange tiling_range = CurrentTilingRange();
      size_t current_tiling_range_offset = current_tiling_ - tiling_range.start;
      return tiling_range.end - 1 - current_tiling_range_offset;
    }
  }
  NOTREACHED();
  return 0;
}

}  // namespace cc
