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
#include "cc/base/rtree.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op_buffer.h"
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

class CC_PAINT_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  DisplayItemList();

  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback = nullptr) const;

  PaintOpBuffer* StartPaint() {
    DCHECK(!in_painting_);
    in_painting_ = true;
    current_range_start_ = paint_op_buffer_.size();
    return &paint_op_buffer_;
  }

  void EndPaintOfUnpaired(const gfx::Rect& visual_rect) {
    if (paint_op_buffer_.size() != current_range_start_) {
      visual_rects_.push_back(visual_rect);
      visual_rects_range_starts_.push_back(current_range_start_);
      GrowCurrentBeginItemVisualRect(visual_rect);
    }

    in_painting_ = false;
  }

  void EndPaintOfPairedBegin(const gfx::Rect& visual_rect = gfx::Rect()) {
    DCHECK_NE(current_range_start_, paint_op_buffer_.size());
    size_t visual_rect_index = visual_rects_.size();
    visual_rects_.push_back(visual_rect);
    visual_rects_range_starts_.push_back(current_range_start_);
    begin_paired_indices_.push_back(visual_rect_index);

    in_painting_ = false;
    in_paired_begin_count_++;
  }

  void EndPaintOfPairedEnd() {
    DCHECK_NE(current_range_start_, paint_op_buffer_.size());
    DCHECK(in_paired_begin_count_);

    // Copy the visual rect of the matching kPairStart.
    size_t last_begin_index = begin_paired_indices_.back();
    begin_paired_indices_.pop_back();
    visual_rects_.push_back(visual_rects_[last_begin_index]);
    visual_rects_range_starts_.push_back(current_range_start_);

    // The block that ended needs to be included in the bounds of the enclosing
    // block.
    GrowCurrentBeginItemVisualRect(visual_rects_[last_begin_index]);

    in_painting_ = false;
    in_paired_begin_count_--;
  }

  // Called after all items are appended, to process the items.
  void Finalize();

  int NumSlowPaths() const { return paint_op_buffer_.numSlowPaths(); }

  // This gives the number of visual rect ranges, which is the number of
  // discrete sets of PaintOps which could be rastered from.
  size_t size() const { return visual_rects_range_starts_.size(); }
  // This gives the total number of PaintOps.
  size_t op_count() const { return paint_op_buffer_.size(); }
  size_t BytesUsed() const;
  bool ShouldBeAnalyzedForSolidColor() const;

  void EmitTraceSnapshot() const;

  void GenerateDiscardableImagesMetadata();
  void GetDiscardableImagesInRect(const gfx::Rect& rect,
                                  float contents_scale,
                                  const gfx::ColorSpace& target_color_space,
                                  std::vector<DrawImage>* images);
  gfx::Rect GetRectForImage(PaintImage::Id image_id) const;

  void SetRetainVisualRectsForTesting(bool retain) {
    retain_visual_rects_ = retain;
  }

  gfx::Rect VisualRectForTesting(int index) { return visual_rects_[index]; }

  void GatherDiscardableImages(DiscardableImageStore* image_store) const;
  const DiscardableImageMap& discardable_image_map_for_testing() const {
    return image_map_;
  }

  bool HasDiscardableImages() const {
    return paint_op_buffer_.HasDiscardableImages();
  }

  // Generate a PaintRecord from this DisplayItemList, leaving |this| in
  // an empty state.
  sk_sp<PaintRecord> ReleaseAsRecord();

 private:
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, AsValueWithNoOps);
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, AsValueWithOps);

  ~DisplayItemList();

  void Reset();

  std::unique_ptr<base::trace_event::TracedValue> CreateTracedValue(
      bool include_items) const;

  // If we're currently within a paired display item block, unions the
  // given visual rect with the begin display item's visual rect.
  void GrowCurrentBeginItemVisualRect(const gfx::Rect& visual_rect);

  RTree rtree_;
  DiscardableImageMap image_map_;
  PaintOpBuffer paint_op_buffer_;

  // The visual rects associated with each of the display items in the
  // display item list. There is one rect per range in
  // visual_rects_range_starts_. These rects are intentionally kept separate
  // because they are used to decide which ops to walk for raster.
  std::vector<gfx::Rect> visual_rects_;
  // For each Rect in visual_rects_, this is the start of the range of
  // PaintOps in the PaintOpBuffer that the Rect describes. The range ends
  // at the start of the next index in the array.
  std::vector<size_t> visual_rects_range_starts_;
  // A stack of indices into the |visual_rects_| for each paired begin range
  // that hasn't been closed.
  std::vector<size_t> begin_paired_indices_;
  // While recording a range of ops, this is the position in the PaintOpBuffer
  // where the recording started.
  size_t current_range_start_ = 0;
  // For debugging, tracks the number of currently nested visual rects being
  // added.
  int in_paired_begin_count_ = 0;
  // For debugging, tracks if we're painting a visual rect range, to prevent
  // nesting.
  bool in_painting_ = false;

  size_t op_count_ = 0u;
  // For testing purposes only. Whether to keep visual rects across calls to
  // Finalize().
  bool retain_visual_rects_ = false;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, BytesUsed);
  DISALLOW_COPY_AND_ASSIGN(DisplayItemList);
};

}  // namespace cc

#endif  // CC_PAINT_DISPLAY_ITEM_LIST_H_
