// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_PRIORITY_H_
#define CC_TILE_PRIORITY_H_

#include <limits>

#include "base/memory/ref_counted.h"
#include "cc/picture_pile.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

enum WhichTree {
  // Note: these must be 0 and 1 because we index with them in various places,
  // e.g. in Tile::priority_.
  ACTIVE_TREE = 0,
  PENDING_TREE = 1,
  NUM_TREES = 2
};

enum TileResolution {
  LOW_RESOLUTION = 0 ,
  HIGH_RESOLUTION = 1,
  NON_IDEAL_RESOLUTION = 2
};

struct CC_EXPORT TilePriority {
  TilePriority()
     : resolution(NON_IDEAL_RESOLUTION),
       time_to_visible_in_seconds(std::numeric_limits<float>::max()),
       time_to_ideal_resolution_in_seconds(std::numeric_limits<float>::max()),
       distance_to_visible_in_pixels(std::numeric_limits<float>::max()) {}

  TilePriority(const TilePriority& active, const TilePriority& pending) {
    if (active.resolution == HIGH_RESOLUTION ||
        pending.resolution == HIGH_RESOLUTION)
      resolution = HIGH_RESOLUTION;
    else if (active.resolution == LOW_RESOLUTION ||
             pending.resolution == LOW_RESOLUTION)
      resolution = LOW_RESOLUTION;
    else
      resolution = NON_IDEAL_RESOLUTION;

    time_to_visible_in_seconds =
      std::min(active.time_to_visible_in_seconds,
               pending.time_to_visible_in_seconds);
    time_to_ideal_resolution_in_seconds =
      std::min(active.time_to_ideal_resolution_in_seconds,
               pending.time_to_ideal_resolution_in_seconds);
    distance_to_visible_in_pixels =
      std::min(active.distance_to_visible_in_pixels,
               pending.distance_to_visible_in_pixels);
  }

  float time_to_needed_in_seconds() const {
    return std::min(time_to_visible_in_seconds,
                    time_to_ideal_resolution_in_seconds);
  }

  static const double kMaxTimeToVisibleInSeconds;

  static int manhattanDistance(const gfx::RectF& a, const gfx::RectF& b);

  // Calculate the time for the |current_bounds| to intersect with the
  // |target_bounds| given its previous location and time delta.
  // This function should work for both scaling and scrolling case.
  static double TimeForBoundsToIntersect(gfx::RectF previous_bounds,
                                         gfx::RectF current_bounds,
                                         double time_delta,
                                         gfx::RectF target_bounds);

  TileResolution resolution;
  float time_to_visible_in_seconds;
  float time_to_ideal_resolution_in_seconds;
  float distance_to_visible_in_pixels;
};

enum TileMemoryLimitPolicy {
  // Nothing.
  ALLOW_NOTHING,

  // You might be made visible, but you're not being interacted with.
  ALLOW_ABSOLUTE_MINIMUM, // Tall.

  // You're being interacted with, but we're low on memory.
  ALLOW_PREPAINT_ONLY, // Grande.

  // You're the only thing in town. Go crazy.
  ALLOW_ANYTHING, // Venti.
};

enum TreePriority {
  SAME_PRIORITY_FOR_BOTH_TREES,
  SMOOTHNESS_TAKES_PRIORITY,
  NEW_CONTENT_TAKES_PRIORITY
};

class GlobalStateThatImpactsTilePriority {
 public:
  GlobalStateThatImpactsTilePriority()
    : memory_limit_policy(ALLOW_NOTHING)
    , memory_limit_in_bytes(0)
    , tree_priority(SAME_PRIORITY_FOR_BOTH_TREES) {
  }

  TileMemoryLimitPolicy memory_limit_policy;

  size_t memory_limit_in_bytes;

  TreePriority tree_priority;
};

}  // namespace cc

#endif  // CC_TILE_PRIORITY_H_
