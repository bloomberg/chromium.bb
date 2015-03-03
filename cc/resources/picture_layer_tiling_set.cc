// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling_set.h"

#include <limits>
#include <set>
#include <vector>

namespace cc {

namespace {

class LargestToSmallestScaleFunctor {
 public:
  bool operator() (PictureLayerTiling* left, PictureLayerTiling* right) {
    return left->contents_scale() > right->contents_scale();
  }
};

inline float LargerRatio(float float1, float float2) {
  DCHECK_GT(float1, 0.f);
  DCHECK_GT(float2, 0.f);
  return float1 > float2 ? float1 / float2 : float2 / float1;
}

}  // namespace

// static
scoped_ptr<PictureLayerTilingSet> PictureLayerTilingSet::Create(
    PictureLayerTilingClient* client,
    size_t max_tiles_for_interest_area,
    float skewport_target_time_in_seconds,
    int skewport_extrapolation_limit_in_content_pixels) {
  return make_scoped_ptr(new PictureLayerTilingSet(
      client, max_tiles_for_interest_area, skewport_target_time_in_seconds,
      skewport_extrapolation_limit_in_content_pixels));
}

PictureLayerTilingSet::PictureLayerTilingSet(
    PictureLayerTilingClient* client,
    size_t max_tiles_for_interest_area,
    float skewport_target_time_in_seconds,
    int skewport_extrapolation_limit_in_content_pixels)
    : max_tiles_for_interest_area_(max_tiles_for_interest_area),
      skewport_target_time_in_seconds_(skewport_target_time_in_seconds),
      skewport_extrapolation_limit_in_content_pixels_(
          skewport_extrapolation_limit_in_content_pixels),
      client_(client) {
}

PictureLayerTilingSet::~PictureLayerTilingSet() {
}

void PictureLayerTilingSet::CopyTilingsFromPendingTwin(
    const PictureLayerTilingSet* pending_twin_set,
    const scoped_refptr<RasterSource>& raster_source) {
  if (pending_twin_set->tilings_.empty()) {
    // If the twin (pending) tiling set is empty, it was not updated for the
    // current frame. So we drop tilings from our set as well, instead of
    // leaving behind unshared tilings that are all non-ideal.
    RemoveAllTilings();
  }

  for (PictureLayerTiling* pending_twin_tiling : pending_twin_set->tilings_) {
    float contents_scale = pending_twin_tiling->contents_scale();
    PictureLayerTiling* this_tiling = FindTilingWithScale(contents_scale);
    if (!this_tiling) {
      scoped_ptr<PictureLayerTiling> new_tiling = PictureLayerTiling::Create(
          contents_scale, raster_source, client_, max_tiles_for_interest_area_,
          skewport_target_time_in_seconds_,
          skewport_extrapolation_limit_in_content_pixels_);
      tilings_.push_back(new_tiling.Pass());
      this_tiling = tilings_.back();
    }
    this_tiling->CloneTilesAndPropertiesFrom(*pending_twin_tiling);
  }
}

void PictureLayerTilingSet::UpdateTilingsToCurrentRasterSource(
    scoped_refptr<RasterSource> raster_source,
    const PictureLayerTilingSet* pending_twin_set,
    const Region& layer_invalidation,
    float minimum_contents_scale,
    float maximum_contents_scale) {
  RemoveTilingsBelowScale(minimum_contents_scale);
  RemoveTilingsAboveScale(maximum_contents_scale);

  // Copy over tilings that are shared with the |pending_twin_set| tiling set
  // (if it exists).
  if (pending_twin_set)
    CopyTilingsFromPendingTwin(pending_twin_set, raster_source);

  // If the tiling is not shared (FindTilingWithScale returns nullptr) or if
  // |this| is the sync set (pending_twin_set is nullptr), then invalidate
  // tiles and update them to the new raster source.
  for (PictureLayerTiling* tiling : tilings_) {
    if (pending_twin_set &&
        pending_twin_set->FindTilingWithScale(tiling->contents_scale())) {
      continue;
    }

    tiling->SetRasterSourceAndResize(raster_source);
    tiling->Invalidate(layer_invalidation);
    tiling->SetRasterSourceOnTiles();
    // This is needed for cases where the live tiles rect didn't change but
    // recordings exist in the raster source that did not exist on the last
    // raster source.
    tiling->CreateMissingTilesInLiveTilesRect();

    // If |pending_twin_set| is present, then |this| is active and |tiling| is
    // not in the pending set, which means it is now NON_IDEAL_RESOLUTION.
    if (pending_twin_set)
      tiling->set_resolution(NON_IDEAL_RESOLUTION);
  }

  tilings_.sort(LargestToSmallestScaleFunctor());

#if DCHECK_IS_ON()
  for (PictureLayerTiling* tiling : tilings_) {
    DCHECK(tiling->tile_size() ==
           client_->CalculateTileSize(tiling->tiling_size()))
        << "tile_size: " << tiling->tile_size().ToString()
        << " tiling_size: " << tiling->tiling_size().ToString()
        << " CalculateTileSize: "
        << client_->CalculateTileSize(tiling->tiling_size()).ToString();
  }

  if (!tilings_.empty()) {
    DCHECK_LE(NumHighResTilings(), 1);
    // When commiting from the main thread the high res tiling may get dropped,
    // but when cloning to the active tree, there should always be one.
    if (pending_twin_set) {
      DCHECK_EQ(1, NumHighResTilings())
          << " num tilings on active: " << tilings_.size()
          << " num tilings on pending: " << pending_twin_set->tilings_.size()
          << " num high res on pending: "
          << pending_twin_set->NumHighResTilings()
          << " are on active tree: " << (client_->GetTree() == ACTIVE_TREE);
    }
  }
#endif
}

void PictureLayerTilingSet::CleanUpTilings(
    float min_acceptable_high_res_scale,
    float max_acceptable_high_res_scale,
    const std::vector<PictureLayerTiling*>& needed_tilings,
    bool should_have_low_res,
    PictureLayerTilingSet* twin_set,
    PictureLayerTilingSet* recycled_twin_set) {
  float twin_low_res_scale = 0.f;
  if (twin_set) {
    PictureLayerTiling* tiling =
        twin_set->FindTilingWithResolution(LOW_RESOLUTION);
    if (tiling)
      twin_low_res_scale = tiling->contents_scale();
  }

  std::vector<PictureLayerTiling*> to_remove;
  for (auto* tiling : tilings_) {
    // Keep all tilings within the min/max scales.
    if (tiling->contents_scale() >= min_acceptable_high_res_scale &&
        tiling->contents_scale() <= max_acceptable_high_res_scale) {
      continue;
    }

    // Keep low resolution tilings, if the tiling set should have them.
    if (should_have_low_res &&
        (tiling->resolution() == LOW_RESOLUTION ||
         tiling->contents_scale() == twin_low_res_scale)) {
      continue;
    }

    // Don't remove tilings that are required.
    if (std::find(needed_tilings.begin(), needed_tilings.end(), tiling) !=
        needed_tilings.end()) {
      continue;
    }

    to_remove.push_back(tiling);
  }

  for (auto* tiling : to_remove) {
    PictureLayerTiling* recycled_twin_tiling =
        recycled_twin_set
            ? recycled_twin_set->FindTilingWithScale(tiling->contents_scale())
            : nullptr;
    // Remove the tiling from the recycle tree. Note that we ignore resolution,
    // since we don't need to maintain high/low res on the recycle set.
    if (recycled_twin_tiling)
      recycled_twin_set->Remove(recycled_twin_tiling);

    DCHECK_NE(HIGH_RESOLUTION, tiling->resolution());
    Remove(tiling);
  }
}

void PictureLayerTilingSet::RemoveNonIdealTilings() {
  auto to_remove = tilings_.remove_if([](PictureLayerTiling* t) {
    return t->resolution() == NON_IDEAL_RESOLUTION;
  });
  tilings_.erase(to_remove, tilings_.end());
}

void PictureLayerTilingSet::MarkAllTilingsNonIdeal() {
  for (auto* tiling : tilings_)
    tiling->set_resolution(NON_IDEAL_RESOLUTION);
}

PictureLayerTiling* PictureLayerTilingSet::AddTiling(
    float contents_scale,
    scoped_refptr<RasterSource> raster_source) {
  for (size_t i = 0; i < tilings_.size(); ++i) {
    DCHECK_NE(tilings_[i]->contents_scale(), contents_scale);
    DCHECK_EQ(tilings_[i]->raster_source(), raster_source.get());
  }

  tilings_.push_back(PictureLayerTiling::Create(
      contents_scale, raster_source, client_, max_tiles_for_interest_area_,
      skewport_target_time_in_seconds_,
      skewport_extrapolation_limit_in_content_pixels_));
  PictureLayerTiling* appended = tilings_.back();

  tilings_.sort(LargestToSmallestScaleFunctor());
  return appended;
}

int PictureLayerTilingSet::NumHighResTilings() const {
  return std::count_if(tilings_.begin(), tilings_.end(),
                       [](PictureLayerTiling* tiling) {
    return tiling->resolution() == HIGH_RESOLUTION;
  });
}

PictureLayerTiling* PictureLayerTilingSet::FindTilingWithScale(
    float scale) const {
  for (size_t i = 0; i < tilings_.size(); ++i) {
    if (tilings_[i]->contents_scale() == scale)
      return tilings_[i];
  }
  return NULL;
}

PictureLayerTiling* PictureLayerTilingSet::FindTilingWithResolution(
    TileResolution resolution) const {
  auto iter = std::find_if(tilings_.begin(), tilings_.end(),
                           [resolution](const PictureLayerTiling* tiling) {
    return tiling->resolution() == resolution;
  });
  if (iter == tilings_.end())
    return NULL;
  return *iter;
}

void PictureLayerTilingSet::RemoveTilingsBelowScale(float minimum_scale) {
  auto to_remove =
      tilings_.remove_if([minimum_scale](PictureLayerTiling* tiling) {
        return tiling->contents_scale() < minimum_scale;
      });
  tilings_.erase(to_remove, tilings_.end());
}

void PictureLayerTilingSet::RemoveTilingsAboveScale(float maximum_scale) {
  auto to_remove =
      tilings_.remove_if([maximum_scale](PictureLayerTiling* tiling) {
        return tiling->contents_scale() > maximum_scale;
      });
  tilings_.erase(to_remove, tilings_.end());
}

void PictureLayerTilingSet::RemoveAllTilings() {
  tilings_.clear();
}

void PictureLayerTilingSet::Remove(PictureLayerTiling* tiling) {
  ScopedPtrVector<PictureLayerTiling>::iterator iter =
    std::find(tilings_.begin(), tilings_.end(), tiling);
  if (iter == tilings_.end())
    return;
  tilings_.erase(iter);
}

void PictureLayerTilingSet::RemoveAllTiles() {
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->Reset();
}

float PictureLayerTilingSet::GetSnappedContentsScale(
    float start_scale,
    float snap_to_existing_tiling_ratio) const {
  // If a tiling exists within the max snapping ratio, snap to its scale.
  float snapped_contents_scale = start_scale;
  float snapped_ratio = snap_to_existing_tiling_ratio;
  for (const auto* tiling : tilings_) {
    float tiling_contents_scale = tiling->contents_scale();
    float ratio = LargerRatio(tiling_contents_scale, start_scale);
    if (ratio < snapped_ratio) {
      snapped_contents_scale = tiling_contents_scale;
      snapped_ratio = ratio;
    }
  }
  return snapped_contents_scale;
}

float PictureLayerTilingSet::GetMaximumContentsScale() const {
  if (tilings_.empty())
    return 0.f;
  // The first tiling has the largest contents scale.
  return tilings_[0]->contents_scale();
}

bool PictureLayerTilingSet::UpdateTilePriorities(
    const gfx::Rect& required_rect_in_layer_space,
    float ideal_contents_scale,
    double current_frame_time_in_seconds,
    const Occlusion& occlusion_in_layer_space,
    bool can_require_tiles_for_activation) {
  bool updated = false;
  for (auto* tiling : tilings_) {
    tiling->set_can_require_tiles_for_activation(
        can_require_tiles_for_activation);
    updated |= tiling->ComputeTilePriorityRects(
        required_rect_in_layer_space, ideal_contents_scale,
        current_frame_time_in_seconds, occlusion_in_layer_space);
  }
  return updated;
}

void PictureLayerTilingSet::GetAllTilesForTracing(
    std::set<const Tile*>* tiles) const {
  for (auto* tiling : tilings_)
    tiling->GetAllTilesForTracing(tiles);
}

PictureLayerTilingSet::CoverageIterator::CoverageIterator(
    const PictureLayerTilingSet* set,
    float contents_scale,
    const gfx::Rect& content_rect,
    float ideal_contents_scale)
    : set_(set),
      contents_scale_(contents_scale),
      ideal_contents_scale_(ideal_contents_scale),
      current_tiling_(-1) {
  missing_region_.Union(content_rect);

  for (ideal_tiling_ = 0;
       static_cast<size_t>(ideal_tiling_) < set_->tilings_.size();
       ++ideal_tiling_) {
    PictureLayerTiling* tiling = set_->tilings_[ideal_tiling_];
    if (tiling->contents_scale() < ideal_contents_scale_) {
      if (ideal_tiling_ > 0)
        ideal_tiling_--;
      break;
    }
  }

  DCHECK_LE(set_->tilings_.size(),
            static_cast<size_t>(std::numeric_limits<int>::max()));

  int num_tilings = set_->tilings_.size();
  if (ideal_tiling_ == num_tilings && ideal_tiling_ > 0)
    ideal_tiling_--;

  ++(*this);
}

PictureLayerTilingSet::CoverageIterator::~CoverageIterator() {
}

gfx::Rect PictureLayerTilingSet::CoverageIterator::geometry_rect() const {
  if (!tiling_iter_) {
    if (!region_iter_.has_rect())
      return gfx::Rect();
    return region_iter_.rect();
  }
  return tiling_iter_.geometry_rect();
}

gfx::RectF PictureLayerTilingSet::CoverageIterator::texture_rect() const {
  if (!tiling_iter_)
    return gfx::RectF();
  return tiling_iter_.texture_rect();
}

Tile* PictureLayerTilingSet::CoverageIterator::operator->() const {
  if (!tiling_iter_)
    return NULL;
  return *tiling_iter_;
}

Tile* PictureLayerTilingSet::CoverageIterator::operator*() const {
  if (!tiling_iter_)
    return NULL;
  return *tiling_iter_;
}

TileResolution PictureLayerTilingSet::CoverageIterator::resolution() const {
  const PictureLayerTiling* tiling = CurrentTiling();
  DCHECK(tiling);
  return tiling->resolution();
}

PictureLayerTiling* PictureLayerTilingSet::CoverageIterator::CurrentTiling()
    const {
  if (current_tiling_ < 0)
    return NULL;
  if (static_cast<size_t>(current_tiling_) >= set_->tilings_.size())
    return NULL;
  return set_->tilings_[current_tiling_];
}

int PictureLayerTilingSet::CoverageIterator::NextTiling() const {
  // Order returned by this method is:
  // 1. Ideal tiling index
  // 2. Tiling index < Ideal in decreasing order (higher res than ideal)
  // 3. Tiling index > Ideal in increasing order (lower res than ideal)
  // 4. Tiling index > tilings.size() (invalid index)
  if (current_tiling_ < 0)
    return ideal_tiling_;
  else if (current_tiling_ > ideal_tiling_)
    return current_tiling_ + 1;
  else if (current_tiling_)
    return current_tiling_ - 1;
  else
    return ideal_tiling_ + 1;
}

PictureLayerTilingSet::CoverageIterator&
PictureLayerTilingSet::CoverageIterator::operator++() {
  bool first_time = current_tiling_ < 0;

  if (!*this && !first_time)
    return *this;

  if (tiling_iter_)
    ++tiling_iter_;

  // Loop until we find a valid place to stop.
  while (true) {
    while (tiling_iter_ &&
           (!*tiling_iter_ || !tiling_iter_->IsReadyToDraw())) {
      missing_region_.Union(tiling_iter_.geometry_rect());
      ++tiling_iter_;
    }
    if (tiling_iter_)
      return *this;

    // If the set of current rects for this tiling is done, go to the next
    // tiling and set up to iterate through all of the remaining holes.
    // This will also happen the first time through the loop.
    if (!region_iter_.has_rect()) {
      current_tiling_ = NextTiling();
      current_region_.Swap(&missing_region_);
      missing_region_.Clear();
      region_iter_ = Region::Iterator(current_region_);

      // All done and all filled.
      if (!region_iter_.has_rect()) {
        current_tiling_ = set_->tilings_.size();
        return *this;
      }

      // No more valid tiles, return this checkerboard rect.
      if (current_tiling_ >= static_cast<int>(set_->tilings_.size()))
        return *this;
    }

    // Pop a rect off.  If there are no more tilings, then these will be
    // treated as geometry with null tiles that the caller can checkerboard.
    gfx::Rect last_rect = region_iter_.rect();
    region_iter_.next();

    // Done, found next checkerboard rect to return.
    if (current_tiling_ >= static_cast<int>(set_->tilings_.size()))
      return *this;

    // Construct a new iterator for the next tiling, but we need to loop
    // again until we get to a valid one.
    tiling_iter_ = PictureLayerTiling::CoverageIterator(
        set_->tilings_[current_tiling_],
        contents_scale_,
        last_rect);
  }

  return *this;
}

PictureLayerTilingSet::CoverageIterator::operator bool() const {
  return current_tiling_ < static_cast<int>(set_->tilings_.size()) ||
      region_iter_.has_rect();
}

void PictureLayerTilingSet::AsValueInto(
    base::trace_event::TracedValue* state) const {
  for (size_t i = 0; i < tilings_.size(); ++i) {
    state->BeginDictionary();
    tilings_[i]->AsValueInto(state);
    state->EndDictionary();
  }
}

size_t PictureLayerTilingSet::GPUMemoryUsageInBytes() const {
  size_t amount = 0;
  for (size_t i = 0; i < tilings_.size(); ++i)
    amount += tilings_[i]->GPUMemoryUsageInBytes();
  return amount;
}

PictureLayerTilingSet::TilingRange PictureLayerTilingSet::GetTilingRange(
    TilingRangeType type) const {
  // Doesn't seem to be the case right now but if it ever becomes a performance
  // problem to compute these ranges each time this function is called, we can
  // compute them only when the tiling set has changed instead.
  TilingRange high_res_range(0, 0);
  TilingRange low_res_range(tilings_.size(), tilings_.size());
  for (size_t i = 0; i < tilings_.size(); ++i) {
    const PictureLayerTiling* tiling = tilings_[i];
    if (tiling->resolution() == HIGH_RESOLUTION)
      high_res_range = TilingRange(i, i + 1);
    if (tiling->resolution() == LOW_RESOLUTION)
      low_res_range = TilingRange(i, i + 1);
  }

  TilingRange range(0, 0);
  switch (type) {
    case HIGHER_THAN_HIGH_RES:
      range = TilingRange(0, high_res_range.start);
      break;
    case HIGH_RES:
      range = high_res_range;
      break;
    case BETWEEN_HIGH_AND_LOW_RES:
      // TODO(vmpstr): This code assumes that high res tiling will come before
      // low res tiling, however there are cases where this assumption is
      // violated. As a result, it's better to be safe in these situations,
      // since otherwise we can end up accessing a tiling that doesn't exist.
      // See crbug.com/429397 for high res tiling appearing after low res
      // tiling discussion/fixes.
      if (high_res_range.start <= low_res_range.start)
        range = TilingRange(high_res_range.end, low_res_range.start);
      else
        range = TilingRange(low_res_range.end, high_res_range.start);
      break;
    case LOW_RES:
      range = low_res_range;
      break;
    case LOWER_THAN_LOW_RES:
      range = TilingRange(low_res_range.end, tilings_.size());
      break;
  }

  DCHECK_LE(range.start, range.end);
  return range;
}

}  // namespace cc
