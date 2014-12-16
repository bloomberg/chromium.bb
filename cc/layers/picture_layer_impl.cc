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
// This must be > 1 as we multiply or divide by this to find a new raster
// scale during pinch.
const float kMaxScaleRatioDuringPinch = 2.0f;

// When creating a new tiling during pinch, snap to an existing
// tiling's scale if the desired scale is within this ratio.
const float kSnapToExistingTilingRatio = 1.2f;

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

PictureLayerImpl::PictureLayerImpl(LayerTreeImpl* tree_impl,
                                   int id,
                                   bool is_mask)
    : LayerImpl(tree_impl, id),
      twin_layer_(nullptr),
      tilings_(CreatePictureLayerTilingSet()),
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
      should_update_tile_priorities_(false),
      only_used_low_res_last_append_quads_(false),
      is_mask_(is_mask),
      nearest_neighbor_(false) {
  layer_tree_impl()->RegisterPictureLayerImpl(this);
}

PictureLayerImpl::~PictureLayerImpl() {
  if (twin_layer_)
    twin_layer_->twin_layer_ = nullptr;
  layer_tree_impl()->UnregisterPictureLayerImpl(this);
}

scoped_ptr<TilingSetEvictionQueue> PictureLayerImpl::CreateEvictionQueue(
    TreePriority tree_priority) {
  if (!tilings_)
    return make_scoped_ptr(new TilingSetEvictionQueue());
  bool skip_shared_out_of_order_tiles =
      GetPendingOrActiveTwinLayer() != nullptr;
  return make_scoped_ptr(new TilingSetEvictionQueue(
      tilings_.get(), tree_priority, skip_shared_out_of_order_tiles));
}

scoped_ptr<TilingSetRasterQueue> PictureLayerImpl::CreateRasterQueue(
    bool prioritize_low_res) {
  if (!tilings_)
    return make_scoped_ptr(new TilingSetRasterQueue());
  return make_scoped_ptr(
      new TilingSetRasterQueue(tilings_.get(), prioritize_low_res));
}

const char* PictureLayerImpl::LayerTypeAsString() const {
  return "cc::PictureLayerImpl";
}

scoped_ptr<LayerImpl> PictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id(), is_mask_);
}

void PictureLayerImpl::PushPropertiesTo(LayerImpl* base_layer) {
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);
  DCHECK_EQ(layer_impl->is_mask_, is_mask_);

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

  layer_impl->SetNearestNeighbor(nearest_neighbor_);

  // Solid color layers have no tilings.
  DCHECK_IMPLIES(raster_source_->IsSolidColor(), tilings_->num_tilings() == 0);
  // The pending tree should only have a high res (and possibly low res) tiling.
  DCHECK_LE(tilings_->num_tilings(),
            layer_tree_impl()->create_low_res_tiling() ? 2u : 1u);

  layer_impl->UpdateRasterSource(raster_source_, &invalidation_,
                                 tilings_.get());
  DCHECK(invalidation_.IsEmpty());

  // After syncing a solid color layer, the active layer has no tilings.
  DCHECK_IMPLIES(raster_source_->IsSolidColor(),
                 layer_impl->tilings_->num_tilings() == 0);

  layer_impl->raster_page_scale_ = raster_page_scale_;
  layer_impl->raster_device_scale_ = raster_device_scale_;
  layer_impl->raster_source_scale_ = raster_source_scale_;
  layer_impl->raster_contents_scale_ = raster_contents_scale_;
  layer_impl->low_res_raster_contents_scale_ = low_res_raster_contents_scale_;

  layer_impl->SanityCheckTilingState();

  // We always need to push properties.
  // See http://crbug.com/303943
  // TODO(danakj): Stop always pushing properties since we don't swap tilings.
  needs_push_properties_ = true;
}

void PictureLayerImpl::AppendQuads(RenderPass* render_pass,
                                   const Occlusion& occlusion_in_content_space,
                                   AppendQuadsData* append_quads_data) {
  // The bounds and the pile size may differ if the pile wasn't updated (ie.
  // PictureLayer::Update didn't happen). In that case the pile will be empty.
  DCHECK_IMPLIES(!raster_source_->GetSize().IsEmpty(),
                 bounds() == raster_source_->GetSize())
      << " bounds " << bounds().ToString() << " pile "
      << raster_source_->GetSize().ToString();

  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();

  if (raster_source_->IsSolidColor()) {
    PopulateSharedQuadState(shared_quad_state);

    AppendDebugBorderQuad(
        render_pass, bounds(), shared_quad_state, append_quads_data);

    SolidColorLayerImpl::AppendSolidQuads(
        render_pass, occlusion_in_content_space, shared_quad_state,
        visible_content_rect(), raster_source_->GetSolidColor(),
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

  shared_quad_state->SetAll(
      scaled_draw_transform, scaled_content_bounds, scaled_visible_content_rect,
      draw_properties().clip_rect, draw_properties().is_clipped,
      draw_properties().opacity, draw_properties().blend_mode,
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
    quad->SetNew(shared_quad_state, geometry_rect, opaque_rect,
                 visible_geometry_rect, texture_rect, texture_size,
                 nearest_neighbor_, RGBA_8888, quad_content_rect,
                 max_contents_scale, raster_source_);
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
                       draw_info.contents_swizzled(),
                       nearest_neighbor_);
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
          quad->SetNew(shared_quad_state, geometry_rect, opaque_rect,
                       visible_geometry_rect, texture_rect, iter.texture_size(),
                       nearest_neighbor_, format, iter->content_rect(),
                       iter->contents_scale(), raster_source_);
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
  DCHECK_EQ(1.f, contents_scale_x());
  DCHECK_EQ(1.f, contents_scale_y());

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
    raster_source_->SetShouldAttemptToUseDistanceFieldText();

  should_update_tile_priorities_ = true;

  UpdateTilePriorities(occlusion_in_content_space);
}

void PictureLayerImpl::UpdateTilePriorities(
    const Occlusion& occlusion_in_content_space) {
  DCHECK_IMPLIES(raster_source_->IsSolidColor(), tilings_->num_tilings() == 0);

  double current_frame_time_in_seconds =
      (layer_tree_impl()->CurrentBeginFrameArgs().frame_time -
       base::TimeTicks()).InSecondsF();
  gfx::Rect viewport_rect_in_layer_space =
      GetViewportForTilePriorityInContentSpace();

  // The tiling set can require tiles for activation any of the following
  // conditions are true:
  // - This layer produced a high-res or non-ideal-res tile last frame.
  // - We're in requires high res to draw mode.
  // - We're not in smoothness takes priority mode.
  // To put different, the tiling set can't require tiles for activation if
  // we're in smoothness mode and only used low-res or checkerboard to draw last
  // frame and we don't need high res to draw.
  //
  // The reason for this is that we should be able to activate sooner and get a
  // more up to date recording, so we don't run out of recording on the active
  // tree.
  bool can_require_tiles_for_activation =
      !only_used_low_res_last_append_quads_ || RequiresHighResToDraw() ||
      !layer_tree_impl()->SmoothnessTakesPriority();

  // Pass |occlusion_in_content_space| for |occlusion_in_layer_space| since
  // they are the same space in picture layer, as contents scale is always 1.
  bool updated = tilings_->UpdateTilePriorities(
      viewport_rect_in_layer_space, ideal_contents_scale_,
      current_frame_time_in_seconds, occlusion_in_content_space,
      can_require_tiles_for_activation);

  // TODO(vmpstr): See if this can be removed in favour of calling it from LTHI
  if (updated)
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

void PictureLayerImpl::UpdateRasterSource(
    scoped_refptr<RasterSource> raster_source,
    Region* new_invalidation,
    const PictureLayerTilingSet* pending_set) {
  // The bounds and the pile size may differ if the pile wasn't updated (ie.
  // PictureLayer::Update didn't happen). In that case the pile will be empty.
  DCHECK_IMPLIES(!raster_source->GetSize().IsEmpty(),
                 bounds() == raster_source->GetSize())
      << " bounds " << bounds().ToString() << " pile "
      << raster_source->GetSize().ToString();

  // The |raster_source_| is initially null, so have to check for that for the
  // first frame.
  bool could_have_tilings = raster_source_.get() && CanHaveTilings();
  raster_source_.swap(raster_source);

  // The |new_invalidation| must be cleared before updating tilings since they
  // access the invalidation through the PictureLayerTilingClient interface.
  invalidation_.Clear();
  invalidation_.Swap(new_invalidation);

  bool can_have_tilings = CanHaveTilings();

  // Need to call UpdateTiles again if CanHaveTilings changed.
  if (could_have_tilings != can_have_tilings)
    layer_tree_impl()->set_needs_update_draw_properties();

  if (!can_have_tilings) {
    RemoveAllTilings();
    return;
  }

  // We could do this after doing UpdateTiles, which would avoid doing this for
  // tilings that are going to disappear on the pending tree (if scale changed).
  // But that would also be more complicated, so we just do it here for now.
  tilings_->UpdateTilingsToCurrentRasterSource(
      raster_source_.get(), pending_set, raster_source_->GetSize(),
      invalidation_, MinimumContentsScale());
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
  raster_source_->DidBeginTracing();
}

void PictureLayerImpl::ReleaseResources() {
  // Recreate tilings with new settings, since some of those might change when
  // we release resources. If tilings_ is null, then leave it as null.
  if (tilings_)
    tilings_ = CreatePictureLayerTilingSet();
  ResetRasterScale();

  // To avoid an edge case after lost context where the tree is up to date but
  // the tilings have not been managed, request an update draw properties
  // to force tilings to get managed.
  layer_tree_impl()->set_needs_update_draw_properties();
}

skia::RefPtr<SkPicture> PictureLayerImpl::GetPicture() {
  return raster_source_->GetFlattenedPicture();
}

scoped_refptr<Tile> PictureLayerImpl::CreateTile(PictureLayerTiling* tiling,
                                               const gfx::Rect& content_rect) {
  DCHECK(!raster_source_->IsSolidColor());
  if (!raster_source_->CoversRect(content_rect, tiling->contents_scale()))
    return scoped_refptr<Tile>();

  int flags = 0;

  // We don't handle solid color masks, so we shouldn't bother analyzing those.
  // Otherwise, always analyze to maximize memory savings.
  if (!is_mask_)
    flags = Tile::USE_PICTURE_ANALYSIS;

  return layer_tree_impl()->tile_manager()->CreateTile(
      raster_source_.get(), content_rect.size(), content_rect,
      tiling->contents_scale(), id(), layer_tree_impl()->source_frame_number(),
      flags);
}

const Region* PictureLayerImpl::GetPendingInvalidation() {
  if (layer_tree_impl()->IsPendingTree())
    return &invalidation_;
  if (layer_tree_impl()->IsRecycleTree())
    return nullptr;
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
  return twin_layer->tilings_->FindTilingWithScale(tiling->contents_scale());
}

PictureLayerTiling* PictureLayerImpl::GetRecycledTwinTiling(
    const PictureLayerTiling* tiling) {
  PictureLayerImpl* recycled_twin = GetRecycledTwinLayer();
  if (!recycled_twin || !recycled_twin->tilings_)
    return nullptr;
  return recycled_twin->tilings_->FindTilingWithScale(tiling->contents_scale());
}

TilePriority::PriorityBin PictureLayerImpl::GetMaxTilePriorityBin() const {
  if (!HasValidTilePriorities())
    return TilePriority::EVENTUALLY;
  return TilePriority::NOW;
}

bool PictureLayerImpl::RequiresHighResToDraw() const {
  return layer_tree_impl()->RequiresHighResToDraw();
}

gfx::Size PictureLayerImpl::CalculateTileSize(
    const gfx::Size& content_bounds) const {
  int max_texture_size =
      layer_tree_impl()->resource_provider()->max_texture_size();

  if (is_mask_) {
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

void PictureLayerImpl::GetContentsResourceId(
    ResourceProvider::ResourceId* resource_id,
    gfx::Size* resource_size) const {
  // The bounds and the pile size may differ if the pile wasn't updated (ie.
  // PictureLayer::Update didn't happen). In that case the pile will be empty.
  DCHECK_IMPLIES(!raster_source_->GetSize().IsEmpty(),
                 bounds() == raster_source_->GetSize())
      << " bounds " << bounds().ToString() << " pile "
      << raster_source_->GetSize().ToString();
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

void PictureLayerImpl::SetNearestNeighbor(bool nearest_neighbor) {
  if (nearest_neighbor_ == nearest_neighbor)
    return;

  nearest_neighbor_ = nearest_neighbor;
  NoteLayerPropertyChanged();
}

PictureLayerTiling* PictureLayerImpl::AddTiling(float contents_scale) {
  DCHECK(CanHaveTilingWithScale(contents_scale)) <<
      "contents_scale: " << contents_scale;

  PictureLayerTiling* tiling =
      tilings_->AddTiling(contents_scale, raster_source_->GetSize());

  DCHECK(raster_source_->HasRecordings());

  return tiling;
}

void PictureLayerImpl::RemoveAllTilings() {
  if (tilings_)
    tilings_->RemoveAllTilings();
  // If there are no tilings, then raster scales are no longer meaningful.
  ResetRasterScale();
}

void PictureLayerImpl::AddTilingsForRasterScale() {
  // Reset all resolution enums on tilings, we'll be setting new values in this
  // function.
  tilings_->MarkAllTilingsNonIdeal();

  PictureLayerTiling* high_res =
      tilings_->FindTilingWithScale(raster_contents_scale_);
  // We always need a high res tiling, so create one if it doesn't exist.
  if (!high_res)
    high_res = AddTiling(raster_contents_scale_);

  // Try and find a low res tiling.
  PictureLayerTiling* low_res = nullptr;
  if (raster_contents_scale_ == low_res_raster_contents_scale_)
    low_res = high_res;
  else
    low_res = tilings_->FindTilingWithScale(low_res_raster_contents_scale_);

  // Only create new low res tilings when the transform is static.  This
  // prevents wastefully creating a paired low res tiling for every new high res
  // tiling during a pinch or a CSS animation.
  bool can_have_low_res = layer_tree_impl()->create_low_res_tiling();
  bool needs_low_res = !low_res;
  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  bool is_animating = draw_properties().screen_space_transform_is_animating;
  if (can_have_low_res && needs_low_res && !is_pinching && !is_animating)
    low_res = AddTiling(low_res_raster_contents_scale_);

  // Set low-res if we have one.
  if (low_res && low_res != high_res)
    low_res->set_resolution(LOW_RESOLUTION);

  // Make sure we always have one high-res (even if high == low).
  high_res->set_resolution(HIGH_RESOLUTION);

  if (layer_tree_impl()->IsPendingTree()) {
    // On the pending tree, drop any tilings that are non-ideal since we don't
    // need them to activate anyway.
    tilings_->RemoveNonIdealTilings();
  }

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
  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (is_pinching && old_raster_contents_scale) {
    // See ShouldAdjustRasterScale:
    // - When zooming out, preemptively create new tiling at lower resolution.
    // - When zooming in, approximate ideal using multiple of kMaxScaleRatio.
    bool zooming_out = old_raster_page_scale > ideal_page_scale_;
    float desired_contents_scale = old_raster_contents_scale;
    if (zooming_out) {
      while (desired_contents_scale > ideal_contents_scale_)
        desired_contents_scale /= kMaxScaleRatioDuringPinch;
    } else {
      while (desired_contents_scale < ideal_contents_scale_)
        desired_contents_scale *= kMaxScaleRatioDuringPinch;
    }
    raster_contents_scale_ = tilings_->GetSnappedContentsScale(
        desired_contents_scale, kSnapToExistingTilingRatio);
    raster_page_scale_ =
        raster_contents_scale_ / raster_device_scale_ / raster_source_scale_;
  }

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
          gfx::ScaleSize(raster_source_->GetSize(), maximum_scale));
      int64 maximum_area = static_cast<int64>(bounds_at_maximum_scale.width()) *
                           static_cast<int64>(bounds_at_maximum_scale.height());
      gfx::Size viewport = layer_tree_impl()->device_viewport_size();
      int64 viewport_area = static_cast<int64>(viewport.width()) *
                            static_cast<int64>(viewport.height());
      if (maximum_area <= viewport_area)
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

  raster_contents_scale_ =
      std::max(raster_contents_scale_, MinimumContentsScale());

  // If this layer would create zero or one tiles at this content scale,
  // don't create a low res tiling.
  gfx::Size raster_bounds = gfx::ToCeiledSize(
      gfx::ScaleSize(raster_source_->GetSize(), raster_contents_scale_));
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
  DCHECK_LE(low_res_raster_contents_scale_, raster_contents_scale_);
  DCHECK_GE(low_res_raster_contents_scale_, MinimumContentsScale());
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

  PictureLayerImpl* twin = GetPendingOrActiveTwinLayer();
  if (twin && twin->CanHaveTilings()) {
    min_acceptable_high_res_scale = std::min(
        min_acceptable_high_res_scale,
        std::min(twin->raster_contents_scale_, twin->ideal_contents_scale_));
    max_acceptable_high_res_scale = std::max(
        max_acceptable_high_res_scale,
        std::max(twin->raster_contents_scale_, twin->ideal_contents_scale_));
  }

  PictureLayerTilingSet* twin_set = twin ? twin->tilings_.get() : nullptr;
  PictureLayerImpl* recycled_twin = GetRecycledTwinLayer();
  PictureLayerTilingSet* recycled_twin_set =
      recycled_twin ? recycled_twin->tilings_.get() : nullptr;

  tilings_->CleanUpTilings(min_acceptable_high_res_scale,
                           max_acceptable_high_res_scale, used_tilings,
                           layer_tree_impl()->create_low_res_tiling(), twin_set,
                           recycled_twin_set);

  if (recycled_twin_set && recycled_twin_set->num_tilings() == 0)
    recycled_twin->ResetRasterScale();

  DCHECK_GT(tilings_->num_tilings(), 0u);
  SanityCheckTilingState();
}

float PictureLayerImpl::MinimumContentsScale() const {
  float setting_min = layer_tree_impl()->settings().minimum_contents_scale;

  // If the contents scale is less than 1 / width (also for height),
  // then it will end up having less than one pixel of content in that
  // dimension.  Bump the minimum contents scale up in this case to prevent
  // this from happening.
  int min_dimension = std::min(raster_source_->GetSize().width(),
                               raster_source_->GetSize().height());
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
  if (raster_source_->IsSolidColor())
    return false;
  if (!DrawsContent())
    return false;
  if (!raster_source_->HasRecordings())
    return false;
  // If the |raster_source_| has a recording it should have non-empty bounds.
  DCHECK(!raster_source_->GetSize().IsEmpty());
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
  float max_contents_scale = tilings_->GetMaximumContentsScale();
  return std::max(max_contents_scale, MinimumContentsScale());
}

scoped_ptr<PictureLayerTilingSet>
PictureLayerImpl::CreatePictureLayerTilingSet() {
  const LayerTreeSettings& settings = layer_tree_impl()->settings();
  return PictureLayerTilingSet::Create(
      this, settings.max_tiles_for_interest_area,
      layer_tree_impl()->use_gpu_rasterization()
          ? 0.f
          : settings.skewport_target_time_in_seconds,
      settings.skewport_extrapolation_limit_in_content_pixels);
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
  tilings_->GetAllTilesForTracing(tiles);
}

void PictureLayerImpl::AsValueInto(base::debug::TracedValue* state) const {
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
  raster_source_->AsValueInto(state);
  state->EndArray();

  state->BeginArray("invalidation");
  invalidation_.AsValueInto(state);
  state->EndArray();

  state->BeginArray("coverage_tiles");
  for (PictureLayerTilingSet::CoverageIterator iter(
           tilings_.get(), 1.f, gfx::Rect(raster_source_->GetSize()),
           ideal_contents_scale_);
       iter; ++iter) {
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

bool PictureLayerImpl::AllTilesRequiredAreReadyToDraw(
    TileRequirementCheck is_tile_required_callback) const {
  if (!HasValidTilePriorities())
    return true;

  if (!tilings_)
    return true;

  if (visible_rect_for_tile_priority_.IsEmpty())
    return true;

  gfx::Rect rect = GetViewportForTilePriorityInContentSpace();
  rect.Intersect(visible_rect_for_tile_priority_);

  // The high resolution tiling is the only tiling that can mark tiles as
  // requiring either draw or activation. There is an explicit check in those
  // callbacks to return false if they are not high resolution tilings. This
  // check needs to remain since there are other callers of that function that
  // rely on it. However, for the purposes of this function, we don't have to
  // check other tilings.
  PictureLayerTiling* tiling =
      tilings_->FindTilingWithResolution(HIGH_RESOLUTION);
  if (!tiling)
    return true;

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
    if ((tiling->*is_tile_required_callback)(tile) && !tile->IsReadyToDraw()) {
      TRACE_EVENT_INSTANT0("cc", "Tile required, but not ready to draw.",
                           TRACE_EVENT_SCOPE_THREAD);
      return false;
    }
  }

  return true;
}

bool PictureLayerImpl::AllTilesRequiredForActivationAreReadyToDraw() const {
  if (!layer_tree_impl()->IsPendingTree())
    return true;

  return AllTilesRequiredAreReadyToDraw(
      &PictureLayerTiling::IsTileRequiredForActivationIfVisible);
}

bool PictureLayerImpl::AllTilesRequiredForDrawAreReadyToDraw() const {
  if (!layer_tree_impl()->IsActiveTree())
    return true;

  return AllTilesRequiredAreReadyToDraw(
      &PictureLayerTiling::IsTileRequiredForDrawIfVisible);
}

}  // namespace cc
