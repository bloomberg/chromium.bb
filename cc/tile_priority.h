// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_PRIORTY_H_
#define CC_TILE_PRIORTY_H_

#include "base/memory/ref_counted.h"
#include "cc/picture_pile.h"
#include "cc/tile_priority.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

struct TilePriority {
  // A given layer may have multiple tilings, of differing quality.
  // would_be_drawn is set for the tiles that are visible that would be
  // drawn if they were chosen.
  bool would_be_drawn;

  // Set to true for tiles that should be favored during, for example,
  // scrolls.
  bool on_primary_tree;

  // Used to prefer tiles near to the viewport.
  float distance_to_viewport;

  // TODO(enne): some metric that penalizes blurriness.
};

class TilePriorityComparator {
 public:
  TilePriorityComparator(bool currently_scrolling)
   : currently_scrolling_(currently_scrolling) {}

  int compare(const TilePriority& a, const TilePriority& b) {
    // TODO(nduca,enne): Implement a comparator using the attributes here.
    return 0;
  }

 private:
  bool currently_scrolling_;
};

}  // namespace cc
#endif
