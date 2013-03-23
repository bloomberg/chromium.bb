// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling_set.h"

namespace cc {

namespace {

class LargestToSmallestScaleFunctor {
 public:
  bool operator() (PictureLayerTiling* left, PictureLayerTiling* right) {
    return left->contents_scale() > right->contents_scale();
  }
};

}  // namespace


PictureLayerTilingSet::PictureLayerTilingSet(
    PictureLayerTilingClient * client)
    : client_(client) {
}

PictureLayerTilingSet::~PictureLayerTilingSet() {
}

void PictureLayerTilingSet::SetClient(PictureLayerTilingClient* client) {
  client_ = client;
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->SetClient(client_);
}

void PictureLayerTilingSet::CloneAll(
    const PictureLayerTilingSet& other,
    const Region& invalidation,
    float minimum_contents_scale) {
  tilings_.clear();
  tilings_.reserve(other.tilings_.size());
  for (size_t i = 0; i < other.tilings_.size(); ++i) {
    if (other.tilings_[i]->contents_scale() < minimum_contents_scale)
      continue;
    Clone(other.tilings_[i], invalidation);
  }
}

void PictureLayerTilingSet::Clone(
    const PictureLayerTiling* tiling,
    const Region& invalidation) {

  for (size_t i = 0; i < tilings_.size(); ++i)
    DCHECK_NE(tilings_[i]->contents_scale(), tiling->contents_scale());

  tilings_.push_back(tiling->Clone());
  gfx::Size size = tilings_.back()->layer_bounds();
  tilings_.back()->SetClient(client_);
  tilings_.back()->Invalidate(invalidation);
  // Intentionally use this set's layer bounds, as it may have changed.
  tilings_.back()->SetLayerBounds(layer_bounds_);

  tilings_.sort(LargestToSmallestScaleFunctor());
}

void PictureLayerTilingSet::SetLayerBounds(gfx::Size layer_bounds) {
  if (layer_bounds_ == layer_bounds)
    return;
  layer_bounds_ = layer_bounds;
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->SetLayerBounds(layer_bounds);
}

gfx::Size PictureLayerTilingSet::LayerBounds() const {
  return layer_bounds_;
}

void PictureLayerTilingSet::Invalidate(const Region& layer_invalidation) {
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->Invalidate(layer_invalidation);
}

PictureLayerTiling* PictureLayerTilingSet::AddTiling(float contents_scale) {
  tilings_.push_back(PictureLayerTiling::Create(contents_scale));
  PictureLayerTiling* appended = tilings_.back();
  appended->SetClient(client_);
  appended->SetLayerBounds(layer_bounds_);

  tilings_.sort(LargestToSmallestScaleFunctor());
  return appended;
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

void PictureLayerTilingSet::CreateTilesFromLayerRect(gfx::Rect layer_rect) {
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->CreateTilesFromLayerRect(layer_rect);
}

PictureLayerTilingSet::Iterator::Iterator(
    const PictureLayerTilingSet* set,
    float contents_scale,
    gfx::Rect content_rect,
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

  if (ideal_tiling_ == set_->tilings_.size() && ideal_tiling_ > 0)
    ideal_tiling_--;

  ++(*this);
}

PictureLayerTilingSet::Iterator::~Iterator() {
}

gfx::Rect PictureLayerTilingSet::Iterator::geometry_rect() const {
  if (!tiling_iter_) {
    if (!region_iter_.has_rect())
      return gfx::Rect();
    return region_iter_.rect();
  }
  return tiling_iter_.geometry_rect();
}

gfx::RectF PictureLayerTilingSet::Iterator::texture_rect() const {
  if (!tiling_iter_)
    return gfx::RectF();
  return tiling_iter_.texture_rect();
}

gfx::Size PictureLayerTilingSet::Iterator::texture_size() const {
  if (!tiling_iter_)
    return gfx::Size();
  return tiling_iter_.texture_size();
}

Tile* PictureLayerTilingSet::Iterator::operator->() const {
  if (!tiling_iter_)
    return NULL;
  return *tiling_iter_;
}

Tile* PictureLayerTilingSet::Iterator::operator*() const {
  if (!tiling_iter_)
    return NULL;
  return *tiling_iter_;
}

PictureLayerTiling* PictureLayerTilingSet::Iterator::CurrentTiling() {
  if (current_tiling_ < 0)
    return NULL;
  if (static_cast<size_t>(current_tiling_) >= set_->tilings_.size())
    return NULL;
  return set_->tilings_[current_tiling_];
}

int PictureLayerTilingSet::Iterator::NextTiling() const {
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

PictureLayerTilingSet::Iterator& PictureLayerTilingSet::Iterator::operator++() {
  bool first_time = current_tiling_ < 0;

  if (!*this && !first_time)
    return *this;

  if (tiling_iter_)
    ++tiling_iter_;

  // Loop until we find a valid place to stop.
  while (true) {
    while (tiling_iter_ &&
           (!*tiling_iter_ || !tiling_iter_->drawing_info().IsReadyToDraw())) {
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
      current_region_.Swap(missing_region_);
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
    tiling_iter_ = PictureLayerTiling::Iterator(
        set_->tilings_[current_tiling_],
        contents_scale_,
        last_rect);
  }

  return *this;
}

PictureLayerTilingSet::Iterator::operator bool() const {
  return current_tiling_ < static_cast<int>(set_->tilings_.size()) ||
      region_iter_.has_rect();
}

void PictureLayerTilingSet::UpdateTilePriorities(
    WhichTree tree,
    gfx::Size device_viewport,
    gfx::Rect viewport_in_content_space,
    gfx::Size last_layer_bounds,
    gfx::Size current_layer_bounds,
    float last_layer_contents_scale,
    float current_layer_contents_scale,
    const gfx::Transform& last_screen_transform,
    const gfx::Transform& current_screen_transform,
    int current_source_frame_number,
    double current_frame_time,
    bool store_screen_space_quads_on_tiles) {
  gfx::RectF viewport_in_layer_space = gfx::ScaleRect(
    viewport_in_content_space,
    1.f / current_layer_contents_scale,
    1.f / current_layer_contents_scale);

  for (size_t i = 0; i < tilings_.size(); ++i) {
    tilings_[i]->UpdateTilePriorities(
        tree,
        device_viewport,
        viewport_in_layer_space,
        last_layer_bounds,
        current_layer_bounds,
        last_layer_contents_scale,
        current_layer_contents_scale,
        last_screen_transform,
        current_screen_transform,
        current_source_frame_number,
        current_frame_time,
        store_screen_space_quads_on_tiles);
  }
}

void PictureLayerTilingSet::DidBecomeActive() {
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->DidBecomeActive();
}

scoped_ptr<base::Value> PictureLayerTilingSet::AsValue() const {
  scoped_ptr<base::ListValue> state(new base::ListValue());
  for (size_t i = 0; i < tilings_.size(); ++i)
    state->Append(tilings_[i]->AsValue().release());
  return state.PassAs<base::Value>();
}

}  // namespace cc
