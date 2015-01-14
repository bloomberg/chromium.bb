// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILING_SET_EVICTION_QUEUE_H_
#define CC_RESOURCES_TILING_SET_EVICTION_QUEUE_H_

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/resources/picture_layer_tiling_set.h"

namespace cc {

// This eviction queue returned tiles from all tilings in a tiling set in
// the following order:
// 1) Eventually rect tiles (EVENTUALLY tiles).
//    1) Eventually rect tiles not required for activation from each tiling in
//       the tiling set, in turn, in the following order:
//       1) the first higher than high res tiling, the second one and so on
//       2) the first lower than low res tiling, the second one and so on
//       3) the first between high and low res tiling, the second one and so on
//       4) low res tiling
//       5) high res tiling
//    2) Eventually rect tiles required for activation from the tiling with
//       required for activation tiles. In the case of a pending tree tiling
//       set that is the high res tiling. In the case of an active tree tiling
//       set that is a tiling whose twin tiling is a pending tree high res
//       tiling.
// 2) Soon border rect and skewport rect tiles (whose priority bin is SOON
//    unless the max tile priority bin is lowered by PictureLayerTilingClient).
//    1) Soon border rect and skewport rect tiles not required for activation
//       from each tiling in the tiling set.
//        * Tilings are iterated in the same order as in the case of eventually
//          rect tiles not required for activation.
//        * For each tiling, first soon border rect tiles and then skewport
//          rect tiles are returned.
//    2) Soon border rect and skewport rect tiles required for activation from
//       the tiling with required for activation tiles.
//        * First soon border rect tiles and then skewport rect tiles are
//          returned.
// 3) Visible rect tiles (whose priority bin is NOW unless the max tile
//    priority bin is lowered by PictureLayerTilingClient).
//    1) Visible rect tiles not required for activation from each tiling in
//       the tiling set.
//        * Tilings are iterated in the same order as in the case of eventually
//          rect tiles not required for activation.
//        * For each tiling, first occluded tiles and then unoccluded tiles
//          are returned.
//    2) Visible rect tiles required for activation from the tiling with
//       required for activation tiles.
//        * First occluded tiles and then unoccluded tiles are returned.
//    If the max tile priority bin is lowered by PictureLayerTilingClient,
//    occlusion is not taken into account as occlusion is meaningful only for
//    NOW tiles.
//
// Within each tiling and tile priority rect, tiles are returned in reverse
// spiral order i.e. in (mostly) decreasing distance-to-visible order.
//
// If the skip_shared_out_of_order_tiles value passed to the constructor is
// true (like it should be when there is a twin layer with a twin tiling set),
// eviction queue does not return shared which are out of order because their
// priority for tree priority is lowered or raised by a twin layer.
//  * If tree_priority is SAME_PRIORITY_FOR_BOTH_TREES, this happens for
//    a tile specific lower priority tree eviction queue (because priority for
//    tree priority is a merged priority).
//  * If tree priority is NEW_CONTENT_TAKES_PRIORITY, this happens for
//    an active tree eviction queue (because priority for tree priority is
//    the pending priority).
//  * If tree_priority is SMOOTHNESS_TAKES_PRIORITY, this happens for a pending
//    tree eviction queue (because priority for tree priority is the active
//    priority).
// Those skipped shared out of order tiles are when returned only by the twin
// eviction queue.
class CC_EXPORT TilingSetEvictionQueue {
 public:
  TilingSetEvictionQueue(PictureLayerTilingSet* tiling_set,
                         TreePriority tree_priority,
                         bool skip_shared_out_of_order_tiles);
  ~TilingSetEvictionQueue();

  Tile* Top();
  const Tile* Top() const;
  void Pop();
  bool IsEmpty() const;

 private:
  bool AdvanceToNextEvictionTile();
  bool AdvanceToNextPriorityBin();
  bool AdvanceToNextTilingRangeType();
  bool AdvanceToNextValidTiling();

  PictureLayerTilingSet::TilingRange CurrentTilingRange() const;
  size_t CurrentTilingIndex() const;
  bool IsSharedOutOfOrderTile(const Tile* tile) const;
  size_t TilingIndexWithRequiredForActivationTiles() const;

  PictureLayerTilingSet* tiling_set_;
  WhichTree tree_;
  TreePriority tree_priority_;
  bool skip_all_shared_tiles_;
  bool skip_shared_out_of_order_tiles_;
  bool processing_soon_border_rect_;
  bool processing_tiling_with_required_for_activation_tiles_;
  size_t tiling_index_with_required_for_activation_tiles_;

  TilePriority::PriorityBin current_priority_bin_;
  PictureLayerTiling* current_tiling_;
  size_t current_tiling_index_;
  PictureLayerTilingSet::TilingRangeType current_tiling_range_type_;
  Tile* current_eviction_tile_;

  TilingData::ReverseSpiralDifferenceIterator spiral_iterator_;
  TilingData::Iterator visible_iterator_;
  std::vector<Tile*> unoccluded_now_tiles_;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILING_SET_EVICTION_QUEUE_H_
