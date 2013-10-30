// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_pile.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "cc/base/region.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/picture_pile_impl.h"

namespace {
// Layout pixel buffer around the visible layer rect to record.  Any base
// picture that intersects the visible layer rect expanded by this distance
// will be recorded.
const int kPixelDistanceToRecord = 8000;
}  // namespace

namespace cc {

PicturePile::PicturePile() {
}

PicturePile::~PicturePile() {
}

bool PicturePile::Update(
    ContentLayerClient* painter,
    SkColor background_color,
    bool contents_opaque,
    const Region& invalidation,
    gfx::Rect visible_layer_rect,
    RenderingStatsInstrumentation* stats_instrumentation) {
  background_color_ = background_color;
  contents_opaque_ = contents_opaque;

  gfx::Rect interest_rect = visible_layer_rect;
  interest_rect.Inset(
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord);

  bool invalidated = false;
  for (Region::Iterator i(invalidation); i.has_rect(); i.next()) {
    gfx::Rect invalidation = i.rect();
    // Split this inflated invalidation across tile boundaries and apply it
    // to all tiles that it touches.
    for (TilingData::Iterator iter(&tiling_, invalidation);
         iter; ++iter) {
      const PictureMapKey& key = iter.index();

      PictureMap::iterator picture_it = picture_map_.find(key);
      if (picture_it == picture_map_.end())
        continue;

      invalidated = picture_it->second.Invalidate() || invalidated;
    }
  }

  gfx::Rect record_rect;
  for (TilingData::Iterator it(&tiling_, interest_rect);
       it; ++it) {
    const PictureMapKey& key = it.index();
    const PictureInfo& info = picture_map_[key];
    if (!info.picture.get()) {
      gfx::Rect tile = PaddedRect(key);
      record_rect.Union(tile);
    }
  }

  if (record_rect.IsEmpty()) {
    if (invalidated)
      UpdateRecordedRegion();
    return invalidated;
  }

  int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
  scoped_refptr<Picture> picture = Picture::Create(record_rect);

  {
    base::TimeDelta best_duration = base::TimeDelta::FromInternalValue(
        std::numeric_limits<int64>::max());
    for (int i = 0; i < repeat_count; i++) {
      base::TimeTicks start_time = stats_instrumentation->StartRecording();
      picture->Record(painter, tile_grid_info_);
      base::TimeDelta duration =
          stats_instrumentation->EndRecording(start_time);
      best_duration = std::min(duration, best_duration);
    }
    int recorded_pixel_count =
        picture->LayerRect().width() * picture->LayerRect().height();
    stats_instrumentation->AddRecord(best_duration, recorded_pixel_count);
    if (num_raster_threads_ > 1)
      picture->GatherPixelRefs(tile_grid_info_);
    picture->CloneForDrawing(num_raster_threads_);
  }

  for (TilingData::Iterator it(&tiling_, record_rect);
       it; ++it) {
    const PictureMapKey& key = it.index();
    gfx::Rect tile = PaddedRect(key);
    if (record_rect.Contains(tile)) {
      PictureInfo& info = picture_map_[key];
      info.picture = picture;
    }
  }

  UpdateRecordedRegion();
  return true;
}

}  // namespace cc
