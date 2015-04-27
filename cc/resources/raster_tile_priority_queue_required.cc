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
  for (const auto& pair : paired_layers) {
    PictureLayerTilingSet* tiling_set = nullptr;
    if (type == Type::REQUIRED_FOR_DRAW && pair.active)
      tiling_set = pair.active->picture_layer_tiling_set();
    else if (type == Type::REQUIRED_FOR_ACTIVATION && pair.pending)
      tiling_set = pair.pending->picture_layer_tiling_set();

    if (!tiling_set)
      continue;

    scoped_ptr<TilingSetRasterQueueRequired> tiling_set_queue(
        new TilingSetRasterQueueRequired(tiling_set, type));
    if (tiling_set_queue->IsEmpty())
      continue;
    tiling_set_queues_.push_back(tiling_set_queue.Pass());
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
