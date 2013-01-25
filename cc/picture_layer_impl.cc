// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_impl.h"

#include "base/time.h"
#include "cc/append_quads_data.h"
#include "cc/checkerboard_draw_quad.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/debug_colors.h"
#include "cc/layer_tree_impl.h"
#include "cc/math_util.h"
#include "cc/quad_sink.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace {
const float kMaxScaleRatioDuringPinch = 2.0f;
}

namespace cc {

PictureLayerImpl::PictureLayerImpl(LayerTreeImpl* treeImpl, int id)
    : LayerImpl(treeImpl, id),
      pile_(PicturePileImpl::Create()),
      last_update_time_(0),
      last_content_scale_(0),
      ideal_contents_scale_(0),
      is_mask_(false) {
}

PictureLayerImpl::~PictureLayerImpl() {
}

const char* PictureLayerImpl::layerTypeAsString() const {
  return "PictureLayer";
}

scoped_ptr<LayerImpl> PictureLayerImpl::createLayerImpl(
    LayerTreeImpl* treeImpl) {
  return PictureLayerImpl::create(treeImpl, id()).PassAs<LayerImpl>();
}

void PictureLayerImpl::CreateTilingSet() {
  DCHECK(layerTreeImpl()->IsPendingTree());
  DCHECK(!tilings_);
  tilings_.reset(new PictureLayerTilingSet(this));
  tilings_->SetLayerBounds(bounds());
}

void PictureLayerImpl::TransferTilingSet(scoped_ptr<PictureLayerTilingSet> tilings) {
  DCHECK(layerTreeImpl()->IsActiveTree());
  tilings->SetClient(this);
  tilings_ = tilings.Pass();
}

void PictureLayerImpl::pushPropertiesTo(LayerImpl* base_layer) {
  LayerImpl::pushPropertiesTo(base_layer);

  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  layer_impl->SetIsMask(is_mask_);
  layer_impl->TransferTilingSet(tilings_.Pass());
  layer_impl->pile_ = pile_;
  pile_ = PicturePileImpl::Create();
  pile_->set_slow_down_raster_scale_factor(
      layerTreeImpl()->debug_state().slowDownRasterScaleFactor);
}


void PictureLayerImpl::appendQuads(QuadSink& quadSink,
                                   AppendQuadsData& appendQuadsData) {
  const gfx::Rect& rect = visibleContentRect();
  gfx::Rect content_rect(gfx::Point(), contentBounds());

  SharedQuadState* sharedQuadState =
      quadSink.useSharedQuadState(createSharedQuadState());
  bool clipped = false;
  gfx::QuadF target_quad = MathUtil::mapQuad(
      drawTransform(),
      gfx::QuadF(rect),
      clipped);
  bool isAxisAlignedInTarget = !clipped && target_quad.IsRectilinear();
  bool useAA = !isAxisAlignedInTarget;

  if (showDebugBorders()) {
    for (PictureLayerTilingSet::Iterator iter(tilings_.get(),
                                              contentsScaleX(),
                                              rect,
                                              ideal_contents_scale_);
         iter;
         ++iter) {
      SkColor color;
      float width;
      if (*iter && iter->GetResourceId()) {
        if (iter->priority(ACTIVE_TREE).resolution == HIGH_RESOLUTION) {
          color = DebugColors::HighResTileBorderColor();
          width = DebugColors::HighResTileBorderWidth(layerTreeImpl());
        } else if (iter->priority(ACTIVE_TREE).resolution == LOW_RESOLUTION) {
          color = DebugColors::LowResTileBorderColor();
          width = DebugColors::LowResTileBorderWidth(layerTreeImpl());
        } else if (iter->contents_scale() > contentsScaleX()) {
          color = DebugColors::ExtraHighResTileBorderColor();
          width = DebugColors::ExtraHighResTileBorderWidth(layerTreeImpl());
        } else {
          color = DebugColors::ExtraLowResTileBorderColor();
          width = DebugColors::ExtraLowResTileBorderWidth(layerTreeImpl());
        }
      } else {
        color = DebugColors::MissingTileBorderColor();
        width = DebugColors::MissingTileBorderWidth(layerTreeImpl());
      }

      scoped_ptr<DebugBorderDrawQuad> debugBorderQuad =
          DebugBorderDrawQuad::Create();
      gfx::Rect geometry_rect = iter.geometry_rect();
      debugBorderQuad->SetNew(sharedQuadState, geometry_rect, color, width);
      quadSink.append(debugBorderQuad.PassAs<DrawQuad>(), appendQuadsData);
    }
  }

  // Keep track of the tilings that were used so that tilings that are
  // unused can be considered for removal.
  std::vector<PictureLayerTiling*> seen_tilings;

  for (PictureLayerTilingSet::Iterator iter(tilings_.get(),
                                            contentsScaleX(),
                                            rect,
                                            ideal_contents_scale_);
       iter;
       ++iter) {
    ResourceProvider::ResourceId resource = 0;
    if (*iter)
      resource = iter->GetResourceId();

    gfx::Rect geometry_rect = iter.geometry_rect();

    if (!resource) {
      if (drawCheckerboardForMissingTiles()) {
        // TODO(enne): Figure out how to show debug "invalidated checker" color
        scoped_ptr<CheckerboardDrawQuad> quad = CheckerboardDrawQuad::Create();
        SkColor color = DebugColors::DefaultCheckerboardColor();
        quad->SetNew(sharedQuadState, geometry_rect, color);
        if (quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData))
            appendQuadsData.numMissingTiles++;
      } else {
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(sharedQuadState, geometry_rect, backgroundColor());
        if (quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData))
            appendQuadsData.numMissingTiles++;
      }
      continue;
    }

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

    if (!seen_tilings.size() || seen_tilings.back() != iter.CurrentTiling())
      seen_tilings.push_back(iter.CurrentTiling());
  }

  // During a pinch, a user could zoom in and out, so throwing away a tiling may
  // be premature.
  if (!layerTreeImpl()->PinchGestureActive())
    CleanUpUnusedTilings(seen_tilings);
}

void PictureLayerImpl::dumpLayerProperties(std::string*, int indent) const {
  // TODO(enne): implement me
}

void PictureLayerImpl::didUpdateTransforms() {
  gfx::Transform current_screen_space_transform =
      screenSpaceTransform();
  double current_time =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  double time_delta = 0;
  if (last_update_time_ != 0 && last_bounds_ == bounds() &&
      last_content_bounds_ == contentBounds() &&
      last_content_scale_ == contentsScaleX()) {
    time_delta = current_time - last_update_time_;
  }
  WhichTree tree = layerTreeImpl()->IsActiveTree() ? ACTIVE_TREE : PENDING_TREE;
  tilings_->UpdateTilePriorities(
      tree,
      layerTreeImpl()->device_viewport_size(),
      last_content_scale_,
      contentsScaleX(),
      last_screen_space_transform_,
      current_screen_space_transform,
      time_delta);

  last_screen_space_transform_ = current_screen_space_transform;
  last_update_time_ = current_time;
  last_bounds_ = bounds();
  last_content_bounds_ = contentBounds();
  last_content_scale_ = contentsScaleX();
}

void PictureLayerImpl::didBecomeActive() {
  LayerImpl::didBecomeActive();
  tilings_->DidBecomeActive();
}

void PictureLayerImpl::didLoseOutputSurface() {
  if (tilings_)
    tilings_->RemoveAllTilings();
}

void PictureLayerImpl::calculateContentsScale(
    float ideal_contents_scale,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  if (!drawsContent()) {
    DCHECK(!tilings_->num_tilings());
    return;
  }

  float min_contents_scale = layerTreeImpl()->settings().minimumContentsScale;
  ideal_contents_scale_ = std::max(ideal_contents_scale, min_contents_scale);

  ManageTilings(ideal_contents_scale_);

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

skia::RefPtr<SkPicture> PictureLayerImpl::getPicture() {
  return pile_->GetFlattenedPicture();
}

scoped_refptr<Tile> PictureLayerImpl::CreateTile(PictureLayerTiling* tiling,
                                                 gfx::Rect content_rect) {
  // Ensure there is a recording for this tile.
  gfx::Rect layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(content_rect, 1.f / tiling->contents_scale()));
  layer_rect.Intersect(gfx::Rect(bounds()));
  if (!pile_->recorded_region().Contains(layer_rect))
    return scoped_refptr<Tile>();

  return make_scoped_refptr(new Tile(
      layerTreeImpl()->tile_manager(),
      pile_.get(),
      content_rect.size(),
      GL_RGBA,
      content_rect,
      tiling->contents_scale()));
}

void PictureLayerImpl::UpdatePile(Tile* tile) {
  tile->set_picture_pile(pile_);
}

void PictureLayerImpl::SyncFromActiveLayer() {
  DCHECK(layerTreeImpl()->IsPendingTree());
  if (!drawsContent())
    return;

  // If there is an active tree version of this layer, get a copy of its
  // tiles.  This needs to be done last, after setting invalidation and the
  // pile.
  PictureLayerImpl* active_twin = static_cast<PictureLayerImpl*>(
      layerTreeImpl()->FindActiveTreeLayerById(id()));
  if (!active_twin)
    return;
  SyncFromActiveLayer(active_twin);
}

void PictureLayerImpl::SyncFromActiveLayer(const PictureLayerImpl* other) {
  tilings_->CloneAll(*other->tilings_, invalidation_);
  DCHECK(bounds() == tilings_->LayerBounds());

  // It's a sad but unfortunate fact that PicturePile tiling edges do not line
  // up with PictureLayerTiling edges.  Tiles can only be added if they are
  // entirely covered by recordings (that may come from multiple PicturePile
  // tiles).  This check happens in this class's CreateTile() call.  Tiles
  // are not removed (even if they cannot be rerecorded) unless they are
  // invalidated.
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
    const PictureLayerTiling* tiling) {
  tilings_->Clone(tiling, invalidation_);
}

void PictureLayerImpl::SetIsMask(bool is_mask) {
  if (is_mask_ == is_mask)
    return;
  is_mask_ = is_mask;
  if (tilings_)
    tilings_->RemoveAllTiles();
}

ResourceProvider::ResourceId PictureLayerImpl::contentsResourceId() const {
  gfx::Rect content_rect(gfx::Point(), contentBounds());
  float scale = contentsScaleX();
  for (PictureLayerTilingSet::Iterator iter(tilings_.get(),
                                            scale,
                                            content_rect,
                                            ideal_contents_scale_);
       iter;
       ++iter) {
    // Mask resource not ready yet.
    if (!*iter || !iter->GetResourceId())
      return 0;
    // Masks only supported if they fit on exactly one tile.
    if (iter.geometry_rect() != content_rect)
      return 0;
    return iter->GetResourceId();
  }
  return 0;
}

bool PictureLayerImpl::areVisibleResourcesReady() const {
  const gfx::Rect& rect = visibleContentRect();

  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings_->tiling_at(i);

    // Ignore non-high resolution tilings.
    if (tiling->resolution() != HIGH_RESOLUTION)
      continue;

    for (PictureLayerTiling::Iterator iter(tiling,
                                           tiling->contents_scale(),
                                           rect);
         iter;
         ++iter) {
      // A null tile (i.e. no recording) is considered "ready".
      if (*iter && !iter->GetResourceId())
        return false;
    }
    return true;
  }
  return false;
}

PictureLayerTiling* PictureLayerImpl::AddTiling(float contents_scale) {
  if (contents_scale < layerTreeImpl()->settings().minimumContentsScale)
    return NULL;

  const Region& recorded = pile_->recorded_region();
  if (recorded.IsEmpty())
    return NULL;

  PictureLayerTiling* tiling = tilings_->AddTiling(
      contents_scale,
      TileSize());

  for (Region::Iterator iter(recorded); iter.has_rect(); iter.next())
    tiling->CreateTilesFromLayerRect(iter.rect());

  // If a new tiling is created on the active tree, sync it to the pending tree
  // so that it can share the same tiles.
  if (layerTreeImpl()->IsPendingTree())
    return tiling;

  PictureLayerImpl* pending_twin = static_cast<PictureLayerImpl*>(
      layerTreeImpl()->FindPendingTreeLayerById(id()));
  if (!pending_twin)
    return tiling;
  DCHECK_EQ(id(), pending_twin->id());
  pending_twin->SyncTiling(tiling);
  return tiling;
}

gfx::Size PictureLayerImpl::TileSize() const {
  if (is_mask_) {
    int max_size = layerTreeImpl()->MaxTextureSize();
    return gfx::Size(
        std::min(max_size, contentBounds().width()),
        std::min(max_size, contentBounds().height()));
  }

  return layerTreeImpl()->settings().defaultTileSize;
}

namespace {

inline float PositiveRatio(float float1, float float2) {
  DCHECK(float1 > 0);
  DCHECK(float2 > 0);
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

void PictureLayerImpl::ManageTilings(float ideal_contents_scale) {
  DCHECK(ideal_contents_scale);
  float low_res_factor = layerTreeImpl()->settings().lowResContentsScaleFactor;
  float low_res_contents_scale = ideal_contents_scale * low_res_factor;

  // Remove any tilings from the pending tree that don't exactly match the
  // contents scale.  The pending tree should always come in crisp.  However,
  // don't do this during a pinch, to avoid throwing away a tiling that should
  // have been kept.
  if (layerTreeImpl()->IsPendingTree() &&
      !layerTreeImpl()->PinchGestureActive()) {
    std::vector<PictureLayerTiling*> remove_list;
    for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
      PictureLayerTiling* tiling = tilings_->tiling_at(i);
      if (tiling->contents_scale() == ideal_contents_scale)
        continue;
      if (tiling->contents_scale() == low_res_contents_scale)
        continue;
      remove_list.push_back(tiling);
    }

    for (size_t i = 0; i < remove_list.size(); ++i)
      tilings_->Remove(remove_list[i]);
  }

  // Find existing tilings closest to ideal high / low res.
  PictureLayerTiling* high_res = NULL;
  PictureLayerTiling* low_res = NULL;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (!high_res || IsCloserToThan(tiling, high_res, ideal_contents_scale))
      high_res = tiling;
    if (!low_res || IsCloserToThan(tiling, low_res, low_res_contents_scale))
      low_res = tiling;

    // Reset all tilings to non-ideal until the end of this function.
    tiling->set_resolution(NON_IDEAL_RESOLUTION);
  }

  // The active tree always has calcDrawProperties called on it first, and
  // any tilings added to the active tree will be synced to the pending tree.
  if (layerTreeImpl()->IsActiveTree() &&
      layerTreeImpl()->PinchGestureActive() && high_res) {
    // If zooming out, if only available high-res tiling is very high
    // resolution, create additional tilings closer to the ideal.
    // When zooming in, add some additional tilings so that content
    // "crisps up" prior to releasing pinch.
    float ratio = PositiveRatio(
        high_res->contents_scale(),
        ideal_contents_scale);
    if (ratio >= kMaxScaleRatioDuringPinch)
      high_res = AddTiling(ideal_contents_scale);
  } else if (layerTreeImpl()->IsActiveTree() ||
             !layerTreeImpl()->PinchGestureActive()) {
    // When not pinching or if no tilings, add exact contents scales.
    // If this pending layer doesn't have an ideal tiling (because it has
    // no active twin), then this will also create one.
    if (!high_res || high_res->contents_scale() != ideal_contents_scale)
      high_res = AddTiling(ideal_contents_scale);
    if (!low_res || low_res->contents_scale() != low_res_contents_scale)
      low_res = AddTiling(low_res_contents_scale);
  }

  if (high_res)
    high_res->set_resolution(HIGH_RESOLUTION);
  if (low_res && low_res != high_res)
    low_res->set_resolution(LOW_RESOLUTION);
}

void PictureLayerImpl::CleanUpUnusedTilings(
    std::vector<PictureLayerTiling*> used_tilings) {
  std::vector<PictureLayerTiling*> to_remove;

  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    // Don't remove the current high or low res tilinig.
    if (tiling->resolution() != NON_IDEAL_RESOLUTION)
      continue;
    if (std::find(used_tilings.begin(), used_tilings.end(), tiling) ==
        used_tilings.end())
      to_remove.push_back(tiling);
  }

  for (size_t i = 0; i < to_remove.size(); ++i)
    tilings_->Remove(to_remove[i]);
}

}  // namespace cc
