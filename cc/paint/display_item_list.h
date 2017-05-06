// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DISPLAY_ITEM_LIST_H_
#define CC_PAINT_DISPLAY_ITEM_LIST_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/contiguous_container.h"
#include "cc/base/rtree.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/display_item.h"
#include "cc/paint/drawing_display_item.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

class SkCanvas;

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {
class DisplayItem;

class CC_PAINT_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  DisplayItemList();

  // TODO(trchen): Deprecated. Apply clip and scale on the canvas instead.
  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback,
              const gfx::Rect& canvas_target_playback_rect,
              float contents_scale) const;

  void Raster(SkCanvas* canvas, SkPicture::AbortCallback* callback) const;

  // Because processing happens in these CreateAndAppend functions, all the set
  // up for the item should be done via the args, which is why the return type
  // needs to be const, to prevent set-after-processing mistakes.

  // Most paired begin item types default to an empty visual rect, which will
  // subsequently be grown as needed to encompass any contained items that draw
  // content, such as drawing or filter items.
  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendPairedBeginItem(Args&&... args) {
    return CreateAndAppendPairedBeginItemWithVisualRect<DisplayItemType>(
        gfx::Rect(), std::forward<Args>(args)...);
  }

  // This method variant is exposed to allow filters to specify their visual
  // rect since they may draw content despite containing no drawing items.
  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendPairedBeginItemWithVisualRect(
      const gfx::Rect& visual_rect,
      Args&&... args) {
    size_t item_index = visual_rects_.size();
    visual_rects_.push_back(visual_rect);
    begin_item_indices_.push_back(item_index);

    return AllocateAndConstruct<DisplayItemType>(std::forward<Args>(args)...);
  }

  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendPairedEndItem(Args&&... args) {
    DCHECK(!begin_item_indices_.empty());
    size_t last_begin_index = begin_item_indices_.back();
    begin_item_indices_.pop_back();

    // Note that we are doing two separate things below:
    //
    // 1. Appending a new rect to the |visual_rects| list associated with
    //    the newly-being-added paired end item, with that visual rect
    //    having same bounds as its paired begin item, referenced via
    //    |last_begin_index|. The paired begin item may or may not be the
    //    current last visual rect in |visual_rects|, and its bounds has
    //    potentially been grown via calls to CreateAndAppendDrawingItem().
    //
    // 2. If there is still a containing paired begin item after closing the
    //    pair ended in this method call, growing that item's visual rect to
    //    incorporate the bounds of the now-finished pair.
    //
    // Thus we're carefully pushing and growing by the visual rect of the
    // paired begin item we're closing in this method call, which is not
    // necessarily the same as |visual_rects.back()|, and given that the
    // |visual_rects| list is mutated in step 1 before step 2, we also can't
    // shorten the reference via a |const auto| reference. We could make a
    // copy of the rect before list mutation, but that would incur copy
    // overhead.

    // Ending bounds match the starting bounds.
    visual_rects_.push_back(visual_rects_[last_begin_index]);

    // The block that ended needs to be included in the bounds of the enclosing
    // block.
    GrowCurrentBeginItemVisualRect(visual_rects_[last_begin_index]);

    return AllocateAndConstruct<DisplayItemType>(std::forward<Args>(args)...);
  }

  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendDrawingItem(
      const gfx::Rect& visual_rect,
      Args&&... args) {
    visual_rects_.push_back(visual_rect);
    GrowCurrentBeginItemVisualRect(visual_rect);

    const auto& item =
        AllocateAndConstruct<DisplayItemType>(std::forward<Args>(args)...);
    has_discardable_images_ |= item.picture->HasDiscardableImages();
    return item;
  }

  // Called after all items are appended, to process the items and, if
  // applicable, create an internally cached SkPicture.
  void Finalize();

  void SetIsSuitableForGpuRasterization(bool is_suitable) {
    all_items_are_suitable_for_gpu_rasterization_ = is_suitable;
  }
  bool IsSuitableForGpuRasterization() const;

  int ApproximateOpCount() const;
  size_t ApproximateMemoryUsage() const;
  bool ShouldBeAnalyzedForSolidColor() const;

  void EmitTraceSnapshot() const;

  void GenerateDiscardableImagesMetadata();
  void GetDiscardableImagesInRect(const gfx::Rect& rect,
                                  float contents_scale,
                                  const gfx::ColorSpace& target_color_space,
                                  std::vector<DrawImage>* images);
  gfx::Rect GetRectForImage(ImageId image_id) const;

  void SetRetainVisualRectsForTesting(bool retain) {
    retain_visual_rects_ = retain;
  }

  size_t size() const { return items_.size(); }

  gfx::Rect VisualRectForTesting(int index) { return visual_rects_[index]; }

  ContiguousContainer<DisplayItem>::const_iterator begin() const {
    return items_.begin();
  }

  ContiguousContainer<DisplayItem>::const_iterator end() const {
    return items_.end();
  }

  void GatherDiscardableImages(DiscardableImageStore* image_store) const;
  const DiscardableImageMap& discardable_image_map_for_testing() const {
    return image_map_;
  }

  bool has_discardable_images() const { return has_discardable_images_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, AsValueWithNoItems);
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, AsValueWithItems);

  ~DisplayItemList();

  std::unique_ptr<base::trace_event::TracedValue> CreateTracedValue(
      bool include_items) const;

  // If we're currently within a paired display item block, unions the
  // given visual rect with the begin display item's visual rect.
  void GrowCurrentBeginItemVisualRect(const gfx::Rect& visual_rect);

  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& AllocateAndConstruct(Args&&... args) {
    auto* item = &items_.AllocateAndConstruct<DisplayItemType>(
        std::forward<Args>(args)...);
    approximate_op_count_ += item->ApproximateOpCount();
    return *item;
  }

  RTree rtree_;
  DiscardableImageMap image_map_;
  ContiguousContainer<DisplayItem> items_;

  // The visual rects associated with each of the display items in the
  // display item list. There is one rect per display item, and the
  // position in |visual_rects| matches the position of the item in
  // |items| . These rects are intentionally kept separate
  // because they are not needed while walking the |items| for raster.
  std::vector<gfx::Rect> visual_rects_;
  std::vector<size_t> begin_item_indices_;

  int approximate_op_count_ = 0;
  bool all_items_are_suitable_for_gpu_rasterization_ = true;
  // For testing purposes only. Whether to keep visual rects across calls to
  // Finalize().
  bool retain_visual_rects_ = false;
  bool has_discardable_images_ = false;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, ApproximateMemoryUsage);
  DISALLOW_COPY_AND_ASSIGN(DisplayItemList);
};

}  // namespace cc

#endif  // CC_PAINT_DISPLAY_ITEM_LIST_H_
