// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_PRIORTY_H_
#define CC_TILE_PRIORTY_H_

#include "base/memory/ref_counted.h"
#include "cc/picture_pile.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

struct TilePriority {
  // Set to true for tiles that should be favored during, for example,
  // scrolls.
  bool on_primary_tree;

  // A given layer may have multiple tilings, of differing quality.
  // would_be_drawn is set for the tiles that are visible that would be
  // drawn if they were chosen.
  bool would_be_drawn;

  // We expect it to be useful soon.
  bool nice_to_have;

  // Used to prefer tiles near to the viewport.
  float distance_to_viewport;

  // TODO(enne): some metric that penalizes blurriness.
};

enum TileMemoryLimitPolicy {
  // Nothing.
  ALLOW_NOTHING,

  // Use as little as possible.
  ALLOW_ONLY_REQUIRED, // On primary tree, would be drawn.

  // Use as little as possible.
  ALLOW_NICE_TO_HAVE, // On either tree, nice to have

  // Use as much memory, up to memory size.
  ALLOW_ANYTHING,
};

class GlobalStateThatImpactsTilePriority {
public:
  GlobalStateThatImpactsTilePriority()
    : memory_limit_policy(ALLOW_NOTHING)
    , memory_limit_in_bytes(0)
    , smoothness_takes_priority(false)
    , pending_tree_frame_number(-1)
    , active_tree_frame_number(-1) {
  }

  TileMemoryLimitPolicy memory_limit_policy;

  size_t memory_limit_in_bytes;

  // Set when scrolling.
  bool smoothness_takes_priority;

  // Use -1 if no tree.
  int pending_tree_frame_number;
  int active_tree_frame_number;
};

class TilePriorityComparator {
 public:
  TilePriorityComparator(GlobalStateThatImpactsTilePriority& global_state)
   : global_state_(global_state) {}

  int compare(const TilePriority& a, const TilePriority& b) {
    // TODO(nduca,enne): Implement a comparator using the attributes here.
    return 0;
  }

 private:
  GlobalStateThatImpactsTilePriority global_state_;
};

}  // namespace cc
#endif
