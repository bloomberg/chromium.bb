// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_PRIORITY_H_
#define CC_RESOURCES_TILE_PRIORITY_H_

#include <algorithm>
#include <limits>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/resources/picture_pile.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace base {
class Value;
}

namespace cc {

enum WhichTree {
  // Note: these must be 0 and 1 because we index with them in various places,
  // e.g. in Tile::priority_.
  ACTIVE_TREE = 0,
  PENDING_TREE = 1,
  NUM_TREES = 2
  // Be sure to update WhichTreeAsValue when adding new fields.
};
scoped_ptr<base::Value> WhichTreeAsValue(
    WhichTree tree);

enum TileResolution {
  LOW_RESOLUTION = 0 ,
  HIGH_RESOLUTION = 1,
  NON_IDEAL_RESOLUTION = 2,
};
scoped_ptr<base::Value> TileResolutionAsValue(
    TileResolution resolution);

struct CC_EXPORT TilePriority {
  TilePriority()
      : resolution(NON_IDEAL_RESOLUTION),
        required_for_activation(false),
        time_to_visible_in_seconds(std::numeric_limits<float>::infinity()),
        distance_to_visible_in_pixels(std::numeric_limits<float>::infinity()) {}

  TilePriority(TileResolution resolution,
               float time_to_visible_in_seconds,
               float distance_to_visible_in_pixels)
      : resolution(resolution),
        required_for_activation(false),
        time_to_visible_in_seconds(time_to_visible_in_seconds),
        distance_to_visible_in_pixels(distance_to_visible_in_pixels) {}

  TilePriority(const TilePriority& active, const TilePriority& pending) {
    if (active.resolution == HIGH_RESOLUTION ||
        pending.resolution == HIGH_RESOLUTION)
      resolution = HIGH_RESOLUTION;
    else if (active.resolution == LOW_RESOLUTION ||
             pending.resolution == LOW_RESOLUTION)
      resolution = LOW_RESOLUTION;
    else
      resolution = NON_IDEAL_RESOLUTION;

    required_for_activation =
        active.required_for_activation || pending.required_for_activation;

    time_to_visible_in_seconds =
      std::min(active.time_to_visible_in_seconds,
               pending.time_to_visible_in_seconds);
    distance_to_visible_in_pixels =
      std::min(active.distance_to_visible_in_pixels,
               pending.distance_to_visible_in_pixels);
  }

  scoped_ptr<base::Value> AsValue() const;

  // Calculate the time for the |current_bounds| to intersect with the
  // |target_bounds| given its previous location and time delta.
  // This function should work for both scaling and scrolling case.
  static float TimeForBoundsToIntersect(const gfx::RectF& previous_bounds,
                                        const gfx::RectF& current_bounds,
                                        float time_delta,
                                        const gfx::RectF& target_bounds);

  bool operator ==(const TilePriority& other) const {
    return resolution == other.resolution &&
        time_to_visible_in_seconds == other.time_to_visible_in_seconds &&
        distance_to_visible_in_pixels == other.distance_to_visible_in_pixels &&
        required_for_activation == other.required_for_activation;
  }

  bool operator !=(const TilePriority& other) const {
    return !(*this == other);
  }

  TileResolution resolution;
  bool required_for_activation;
  float time_to_visible_in_seconds;
  float distance_to_visible_in_pixels;
};

enum TileMemoryLimitPolicy {
  // Nothing.
  ALLOW_NOTHING = 0,

  // You might be made visible, but you're not being interacted with.
  ALLOW_ABSOLUTE_MINIMUM = 1,  // Tall.

  // You're being interacted with, but we're low on memory.
  ALLOW_PREPAINT_ONLY = 2,  // Grande.

  // You're the only thing in town. Go crazy.
  ALLOW_ANYTHING = 3,  // Venti.

  NUM_TILE_MEMORY_LIMIT_POLICIES = 4,

  // NOTE: Be sure to update TreePriorityAsValue and kBinPolicyMap when adding
  // or reordering fields.
};
scoped_ptr<base::Value> TileMemoryLimitPolicyAsValue(
    TileMemoryLimitPolicy policy);

enum TreePriority {
  SAME_PRIORITY_FOR_BOTH_TREES,
  SMOOTHNESS_TAKES_PRIORITY,
  NEW_CONTENT_TAKES_PRIORITY

  // Be sure to update TreePriorityAsValue when adding new fields.
};
scoped_ptr<base::Value> TreePriorityAsValue(TreePriority prio);

class GlobalStateThatImpactsTilePriority {
 public:
  GlobalStateThatImpactsTilePriority()
      : memory_limit_policy(ALLOW_NOTHING),
        memory_limit_in_bytes(0),
        unused_memory_limit_in_bytes(0),
        num_resources_limit(0),
        tree_priority(SAME_PRIORITY_FOR_BOTH_TREES) {}

  TileMemoryLimitPolicy memory_limit_policy;

  size_t memory_limit_in_bytes;
  size_t unused_memory_limit_in_bytes;
  size_t num_resources_limit;

  TreePriority tree_priority;

  bool operator==(const GlobalStateThatImpactsTilePriority& other) const {
    return memory_limit_policy == other.memory_limit_policy
        && memory_limit_in_bytes == other.memory_limit_in_bytes
        && unused_memory_limit_in_bytes == other.unused_memory_limit_in_bytes
        && num_resources_limit == other.num_resources_limit
        && tree_priority == other.tree_priority;
  }
  bool operator!=(const GlobalStateThatImpactsTilePriority& other) const {
    return !(*this == other);
  }

  scoped_ptr<base::Value> AsValue() const;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_PRIORITY_H_
