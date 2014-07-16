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
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/tile_priority.h"

namespace {
// Layout pixel buffer around the visible layer rect to record.  Any base
// picture that intersects the visible layer rect expanded by this distance
// will be recorded.
const int kPixelDistanceToRecord = 8000;

// TODO(humper): The density threshold here is somewhat arbitrary; need a
// way to set // this from the command line so we can write a benchmark
// script and find a sweet spot.
const float kDensityThreshold = 0.5f;

bool rect_sort_y(const gfx::Rect& r1, const gfx::Rect& r2) {
  return r1.y() < r2.y() || (r1.y() == r2.y() && r1.x() < r2.x());
}

bool rect_sort_x(const gfx::Rect& r1, const gfx::Rect& r2) {
  return r1.x() < r2.x() || (r1.x() == r2.x() && r1.y() < r2.y());
}

float PerformClustering(const std::vector<gfx::Rect>& tiles,
                        std::vector<gfx::Rect>* clustered_rects) {
  // These variables track the record area and invalid area
  // for the entire clustering
  int total_record_area = 0;
  int total_invalid_area = 0;

  // These variables track the record area and invalid area
  // for the current cluster being constructed.
  gfx::Rect cur_record_rect;
  int cluster_record_area = 0, cluster_invalid_area = 0;

  for (std::vector<gfx::Rect>::const_iterator it = tiles.begin();
        it != tiles.end();
        it++) {
    gfx::Rect invalid_tile = *it;

    // For each tile, we consider adding the invalid tile to the
    // current record rectangle.  Only add it if the amount of empty
    // space created is below a density threshold.
    int tile_area = invalid_tile.width() * invalid_tile.height();

    gfx::Rect proposed_union = cur_record_rect;
    proposed_union.Union(invalid_tile);
    int proposed_area = proposed_union.width() * proposed_union.height();
    float proposed_density =
      static_cast<float>(cluster_invalid_area + tile_area) /
      static_cast<float>(proposed_area);

    if (proposed_density >= kDensityThreshold) {
      // It's okay to add this invalid tile to the
      // current recording rectangle.
      cur_record_rect = proposed_union;
      cluster_record_area = proposed_area;
      cluster_invalid_area += tile_area;
      total_invalid_area += tile_area;
    } else {
      // Adding this invalid tile to the current recording rectangle
      // would exceed our badness threshold, so put the current rectangle
      // in the list of recording rects, and start a new one.
      clustered_rects->push_back(cur_record_rect);
      total_record_area += cluster_record_area;
      cur_record_rect = invalid_tile;
      cluster_invalid_area = tile_area;
      cluster_record_area = tile_area;
    }
  }

  DCHECK(!cur_record_rect.IsEmpty());
  clustered_rects->push_back(cur_record_rect);
  total_record_area += cluster_record_area;;

  DCHECK_NE(total_record_area, 0);

  return static_cast<float>(total_invalid_area) /
         static_cast<float>(total_record_area);
}

float ClusterTiles(const std::vector<gfx::Rect>& invalid_tiles,
                   std::vector<gfx::Rect>* record_rects) {
  TRACE_EVENT1("cc", "ClusterTiles",
               "count",
               invalid_tiles.size());

  if (invalid_tiles.size() <= 1) {
    // Quickly handle the special case for common
    // single-invalidation update, and also the less common
    // case of no tiles passed in.
    *record_rects = invalid_tiles;
    return 1;
  }

  // Sort the invalid tiles by y coordinate.
  std::vector<gfx::Rect> invalid_tiles_vertical = invalid_tiles;
  std::sort(invalid_tiles_vertical.begin(),
            invalid_tiles_vertical.end(),
            rect_sort_y);

  float vertical_density;
  std::vector<gfx::Rect> vertical_clustering;
  vertical_density = PerformClustering(invalid_tiles_vertical,
                                       &vertical_clustering);

  // If vertical density is optimal, then we can return early.
  if (vertical_density == 1.f) {
    *record_rects = vertical_clustering;
    return vertical_density;
  }

  // Now try again with a horizontal sort, see which one is best
  std::vector<gfx::Rect> invalid_tiles_horizontal = invalid_tiles;
  std::sort(invalid_tiles_horizontal.begin(),
            invalid_tiles_horizontal.end(),
            rect_sort_x);

  float horizontal_density;
  std::vector<gfx::Rect> horizontal_clustering;
  horizontal_density = PerformClustering(invalid_tiles_horizontal,
                                         &horizontal_clustering);

  if (vertical_density < horizontal_density) {
    *record_rects = horizontal_clustering;
    return horizontal_density;
  }

  *record_rects = vertical_clustering;
  return vertical_density;
}

}  // namespace

namespace cc {

PicturePile::PicturePile() : is_suitable_for_gpu_rasterization_(true) {}

PicturePile::~PicturePile() {
}

bool PicturePile::UpdateAndExpandInvalidation(
    ContentLayerClient* painter,
    Region* invalidation,
    SkColor background_color,
    bool contents_opaque,
    bool contents_fill_bounds_completely,
    const gfx::Size& layer_size,
    const gfx::Rect& visible_layer_rect,
    int frame_number,
    Picture::RecordingMode recording_mode,
    RenderingStatsInstrumentation* stats_instrumentation) {
  background_color_ = background_color;
  contents_opaque_ = contents_opaque;
  contents_fill_bounds_completely_ = contents_fill_bounds_completely;

  bool updated = false;

  Region resize_invalidation;
  gfx::Size old_tiling_size = tiling_size();
  if (old_tiling_size != layer_size) {
    tiling_.SetTilingSize(layer_size);
    updated = true;
  }

  gfx::Rect interest_rect = visible_layer_rect;
  interest_rect.Inset(
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord);
  recorded_viewport_ = interest_rect;
  recorded_viewport_.Intersect(gfx::Rect(tiling_size()));

  gfx::Rect interest_rect_over_tiles =
      tiling_.ExpandRectToTileBounds(interest_rect);

  if (old_tiling_size != layer_size) {
    has_any_recordings_ = false;

    // Drop recordings that are outside the new layer bounds or that changed
    // size.
    std::vector<PictureMapKey> to_erase;
    int min_toss_x = tiling_.num_tiles_x();
    if (tiling_size().width() > old_tiling_size.width()) {
      min_toss_x =
          tiling_.FirstBorderTileXIndexFromSrcCoord(old_tiling_size.width());
    }
    int min_toss_y = tiling_.num_tiles_y();
    if (tiling_size().height() > old_tiling_size.height()) {
      min_toss_y =
          tiling_.FirstBorderTileYIndexFromSrcCoord(old_tiling_size.height());
    }
    for (PictureMap::const_iterator it = picture_map_.begin();
         it != picture_map_.end();
         ++it) {
      const PictureMapKey& key = it->first;
      if (key.first < min_toss_x && key.second < min_toss_y) {
        has_any_recordings_ |= !!it->second.GetPicture();
        continue;
      }
      to_erase.push_back(key);
    }

    for (size_t i = 0; i < to_erase.size(); ++i)
      picture_map_.erase(to_erase[i]);

    // If a recording is dropped and not re-recorded below, invalidate that
    // full recording to cause any raster tiles that would use it to be
    // dropped.
    // If the recording will be replaced below, just invalidate newly exposed
    // areas to force raster tiles that include the old recording to know
    // there is new recording to display.
    gfx::Rect old_tiling_rect_over_tiles =
        tiling_.ExpandRectToTileBounds(gfx::Rect(old_tiling_size));
    if (min_toss_x < tiling_.num_tiles_x()) {
      int unrecorded_left = std::max(tiling_.TilePositionX(min_toss_x),
                                     interest_rect_over_tiles.right());
      int exposed_left = old_tiling_size.width();
      int left = std::min(unrecorded_left, exposed_left);
      int tile_right =
          tiling_.TilePositionX(min_toss_x) + tiling_.TileSizeX(min_toss_x);
      int exposed_right = tiling_size().width();
      int right = std::min(tile_right, exposed_right);
      gfx::Rect right_side(left,
                           old_tiling_rect_over_tiles.y(),
                           right - left,
                           old_tiling_rect_over_tiles.height());
      resize_invalidation.Union(right_side);
    }
    if (min_toss_y < tiling_.num_tiles_y()) {
      int unrecorded_top = std::max(tiling_.TilePositionY(min_toss_y),
                                    interest_rect_over_tiles.bottom());
      int exposed_top = old_tiling_size.height();
      int top = std::min(unrecorded_top, exposed_top);
      int tile_bottom =
          tiling_.TilePositionY(min_toss_y) + tiling_.TileSizeY(min_toss_y);
      int exposed_bottom = tiling_size().height();
      int bottom = std::min(tile_bottom, exposed_bottom);
      gfx::Rect bottom_side(old_tiling_rect_over_tiles.x(),
                            top,
                            old_tiling_rect_over_tiles.width(),
                            bottom - top);
      resize_invalidation.Union(bottom_side);
    }
  }

  Region invalidation_expanded_to_full_tiles;
  for (Region::Iterator i(*invalidation); i.has_rect(); i.next()) {
    gfx::Rect invalid_rect = i.rect();

    // Expand invalidation that is outside tiles that intersect the interest
    // rect. These tiles are no longer valid and should be considerered fully
    // invalid, so we can know to not keep around raster tiles that intersect
    // with these recording tiles.
    gfx::Rect invalid_rect_outside_interest_rect_tiles = invalid_rect;
    // TODO(danakj): We should have a Rect-subtract-Rect-to-2-rects operator
    // instead of using Rect::Subtract which gives you the bounding box of the
    // subtraction.
    invalid_rect_outside_interest_rect_tiles.Subtract(interest_rect_over_tiles);
    invalidation_expanded_to_full_tiles.Union(tiling_.ExpandRectToTileBounds(
        invalid_rect_outside_interest_rect_tiles));

    // Split this inflated invalidation across tile boundaries and apply it
    // to all tiles that it touches.
    bool include_borders = true;
    for (TilingData::Iterator iter(&tiling_, invalid_rect, include_borders);
         iter;
         ++iter) {
      const PictureMapKey& key = iter.index();

      PictureMap::iterator picture_it = picture_map_.find(key);
      if (picture_it == picture_map_.end())
        continue;

      // Inform the grid cell that it has been invalidated in this frame.
      updated = picture_it->second.Invalidate(frame_number) || updated;
      // Invalidate drops the picture so the whole tile better be invalidated if
      // it won't be re-recorded below.
      DCHECK(
          tiling_.TileBounds(key.first, key.second).Intersects(interest_rect) ||
          invalidation_expanded_to_full_tiles.Contains(
              tiling_.TileBounds(key.first, key.second)));
    }
  }

  invalidation->Union(invalidation_expanded_to_full_tiles);
  invalidation->Union(resize_invalidation);

  // Make a list of all invalid tiles; we will attempt to
  // cluster these into multiple invalidation regions.
  std::vector<gfx::Rect> invalid_tiles;
  bool include_borders = true;
  for (TilingData::Iterator it(&tiling_, interest_rect, include_borders); it;
       ++it) {
    const PictureMapKey& key = it.index();
    PictureInfo& info = picture_map_[key];

    gfx::Rect rect = PaddedRect(key);
    int distance_to_visible =
        rect.ManhattanInternalDistance(visible_layer_rect);

    if (info.NeedsRecording(frame_number, distance_to_visible)) {
      gfx::Rect tile = tiling_.TileBounds(key.first, key.second);
      invalid_tiles.push_back(tile);
    } else if (!info.GetPicture()) {
      if (recorded_viewport_.Intersects(rect)) {
        // Recorded viewport is just an optimization for a fully recorded
        // interest rect.  In this case, a tile in that rect has declined
        // to be recorded (probably due to frequent invalidations).
        // TODO(enne): Shrink the recorded_viewport_ rather than clearing.
        recorded_viewport_ = gfx::Rect();
      }

      // If a tile in the interest rect is not recorded, the entire tile needs
      // to be considered invalid, so that we know not to keep around raster
      // tiles that intersect this recording tile.
      invalidation->Union(tiling_.TileBounds(it.index_x(), it.index_y()));
    }
  }

  std::vector<gfx::Rect> record_rects;
  ClusterTiles(invalid_tiles, &record_rects);

  if (record_rects.empty())
    return updated;

  for (std::vector<gfx::Rect>::iterator it = record_rects.begin();
       it != record_rects.end();
       it++) {
    gfx::Rect record_rect = *it;
    record_rect = PadRect(record_rect);

    int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
    scoped_refptr<Picture> picture;
    int num_raster_threads = RasterWorkerPool::GetNumRasterThreads();

    // Note: Currently, gathering of pixel refs when using a single
    // raster thread doesn't provide any benefit. This might change
    // in the future but we avoid it for now to reduce the cost of
    // Picture::Create.
    bool gather_pixel_refs = num_raster_threads > 1;

    {
      base::TimeDelta best_duration = base::TimeDelta::Max();
      for (int i = 0; i < repeat_count; i++) {
        base::TimeTicks start_time = stats_instrumentation->StartRecording();
        picture = Picture::Create(record_rect,
                                  painter,
                                  tile_grid_info_,
                                  gather_pixel_refs,
                                  num_raster_threads,
                                  recording_mode);
        // Note the '&&' with previous is-suitable state.
        // This means that once a picture-pile becomes unsuitable for gpu
        // rasterization due to some content, it will continue to be unsuitable
        // even if that content is replaced by gpu-friendly content.
        // This is an optimization to avoid iterating though all pictures in
        // the pile after each invalidation.
        is_suitable_for_gpu_rasterization_ &=
            picture->IsSuitableForGpuRasterization();
        base::TimeDelta duration =
            stats_instrumentation->EndRecording(start_time);
        best_duration = std::min(duration, best_duration);
      }
      int recorded_pixel_count =
          picture->LayerRect().width() * picture->LayerRect().height();
      stats_instrumentation->AddRecord(best_duration, recorded_pixel_count);
    }

    bool found_tile_for_recorded_picture = false;

    bool include_borders = true;
    for (TilingData::Iterator it(&tiling_, record_rect, include_borders); it;
         ++it) {
      const PictureMapKey& key = it.index();
      gfx::Rect tile = PaddedRect(key);
      if (record_rect.Contains(tile)) {
        PictureInfo& info = picture_map_[key];
        info.SetPicture(picture);
        found_tile_for_recorded_picture = true;
      }
    }
    DCHECK(found_tile_for_recorded_picture);
  }

  has_any_recordings_ = true;
  DCHECK(CanRasterSlowTileCheck(recorded_viewport_));
  return true;
}

void PicturePile::SetEmptyBounds() {
  tiling_.SetTilingSize(gfx::Size());
  picture_map_.clear();
  has_any_recordings_ = false;
  recorded_viewport_ = gfx::Rect();
}

}  // namespace cc
