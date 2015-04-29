// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_tile_priority_queue_required.h"

#include "cc/resources/tiling_set_raster_queue_required.h"

namespace cc {

RasterTilePriorityQueueRequired::RasterTilePriorityQueueRequired() {
}

RasterTilePriorityQueueRequired::~RasterTilePriorityQueueRequired() {
}

void RasterTilePriorityQueueRequired::Build(
    const std::vector<PictureLayerImpl::Pair>& paired_layers,
    Type type) {
  DCHECK_NE(static_cast<int>(type), static_cast<int>(Type::ALL));
  if (type == Type::REQUIRED_FOR_DRAW)
    BuildRequiredForDraw(paired_layers);
  else
    BuildRequiredForActivation(paired_layers);
}

void RasterTilePriorityQueueRequired::BuildRequiredForDraw(
    const std::vector<PictureLayerImpl::Pair>& paired_layers) {
  for (const auto& pair : paired_layers) {
    if (!pair.active)
      continue;
    scoped_ptr<TilingSetRasterQueueRequired> tiling_set_queue(
        new TilingSetRasterQueueRequired(
            pair.active->picture_layer_tiling_set(), Type::REQUIRED_FOR_DRAW));
    if (tiling_set_queue->IsEmpty())
      continue;
    tiling_set_queues_.push_back(tiling_set_queue.Pass());
  }
}

void RasterTilePriorityQueueRequired::BuildRequiredForActivation(
    const std::vector<PictureLayerImpl::Pair>& paired_layers) {
  for (const auto& pair : paired_layers) {
    if (pair.active) {
      scoped_ptr<TilingSetRasterQueueRequired> tiling_set_queue(
          new TilingSetRasterQueueRequired(
              pair.active->picture_layer_tiling_set(),
              Type::REQUIRED_FOR_ACTIVATION));
      if (!tiling_set_queue->IsEmpty())
        tiling_set_queues_.push_back(tiling_set_queue.Pass());
    }
    if (pair.pending) {
      scoped_ptr<TilingSetRasterQueueRequired> tiling_set_queue(
          new TilingSetRasterQueueRequired(
              pair.pending->picture_layer_tiling_set(),
              Type::REQUIRED_FOR_ACTIVATION));
      if (!tiling_set_queue->IsEmpty())
        tiling_set_queues_.push_back(tiling_set_queue.Pass());
    }
  }
}

bool RasterTilePriorityQueueRequired::IsEmpty() const {
  return tiling_set_queues_.empty();
}

Tile* RasterTilePriorityQueueRequired::Top() {
  DCHECK(!IsEmpty());
  return tiling_set_queues_.back()->Top();
}

void RasterTilePriorityQueueRequired::Pop() {
  DCHECK(!IsEmpty());
  tiling_set_queues_.back()->Pop();
  if (tiling_set_queues_.back()->IsEmpty())
    tiling_set_queues_.pop_back();
}

}  // namespace cc
