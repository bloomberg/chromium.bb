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

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {

// DisplayItemList is a container of paint operations. One can populate the list
// using StartPaint, followed by push{,_with_data,_with_array} functions
// specialized with ops coming from paint_op_buffer.h. Internally, the
// DisplayItemList contains a PaintOpBuffer and defers op saving to it.
// Additionally, it store some meta information about the paint operations.
// Specifically, it creates an rtree to assist in rasterization: when
// rasterizing a rect, it queries the rtree to extract only the byte offsets of
// the ops required and replays those into a canvas.
class CC_PAINT_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  // TODO(vmpstr): It would be cool if we didn't need this, and instead used
  // PaintOpBuffer directly when we needed to release this as a paint op buffer.
  enum UsageHint { kTopLevelDisplayItemList, kToBeReleasedAsPaintOpBuffer };

  explicit DisplayItemList(UsageHint = kTopLevelDisplayItemList);

  void Raster(SkCanvas* canvas,
              ImageProvider* image_provider = nullptr,
              SkPicture::AbortCallback* callback = nullptr) const;

  // TODO(vmpstr): This is only used to keep track of debugging info, so we can
  // probably remove it? But it would be nice to delimit painting in a block
  // somehow (RAII object maybe).
  void StartPaint() {
    DCHECK(!in_painting_);
    in_painting_ = true;
  }

  // Push functions construct a new op on the paint op buffer, while maintaining
  // bookkeeping information. Must be called after invoking StartPaint().
  template <typename T, typename... Args>
  void push(Args&&... args) {
    DCHECK(in_painting_);
    if (usage_hint_ == kTopLevelDisplayItemList)
      offsets_.push_back(paint_op_buffer_.next_op_offset());
    paint_op_buffer_.push<T>(std::forward<Args>(args)...);
  }

  void EndPaintOfUnpaired(const gfx::Rect& visual_rect) {
    in_painting_ = false;
    if (usage_hint_ == kToBeReleasedAsPaintOpBuffer)
      return;

    // Empty paint item.
    if (visual_rects_.size() == paint_op_buffer_.size())
      return;

    while (visual_rects_.size() < paint_op_buffer_.size())
      visual_rects_.push_back(visual_rect);
    GrowCurrentBeginItemVisualRect(visual_rect);
  }

  void EndPaintOfPairedBegin(const gfx::Rect& visual_rect = gfx::Rect()) {
    in_painting_ = false;
    if (usage_hint_ == kToBeReleasedAsPaintOpBuffer)
      return;
    DCHECK_NE(visual_rects_.size(), paint_op_buffer_.size());
    size_t count = paint_op_buffer_.size() - visual_rects_.size();
    for (size_t i = 0; i < count; ++i)
      visual_rects_.push_back(visual_rect);
    begin_paired_indices_.push_back(
        std::make_pair(visual_rects_.size() - 1, count));

    in_paired_begin_count_++;
  }

  void EndPaintOfPairedEnd() {
    in_painting_ = false;
    if (usage_hint_ == kToBeReleasedAsPaintOpBuffer)
      return;
    DCHECK_NE(current_range_start_, paint_op_buffer_.size());
    DCHECK(in_paired_begin_count_);

    size_t last_begin_index = begin_paired_indices_.back().first;
    size_t last_begin_count = begin_paired_indices_.back().second;
    DCHECK_GT(last_begin_count, 0u);
    DCHECK_GE(last_begin_index, last_begin_count - 1);

    // Copy the visual rect at |last_begin_index| to all indices that constitute
    // the begin item. Note that because we possibly reallocate the
    // |visual_rects_| buffer below, we need an actual copy instead of a const
    // reference which can become dangling.
    auto visual_rect = visual_rects_[last_begin_index];
    for (size_t i = last_begin_index - last_begin_count + 1;
         i < last_begin_index; ++i) {
      visual_rects_[i] = visual_rect;
    }
    begin_paired_indices_.pop_back();

    // Copy the visual rect of the matching begin item to the end item(s).
    while (visual_rects_.size() < paint_op_buffer_.size())
      visual_rects_.push_back(visual_rect);

    // The block that ended needs to be included in the bounds of the enclosing
    // block.
    GrowCurrentBeginItemVisualRect(visual_rect);

    in_paired_begin_count_--;
  }

  // Called after all items are appended, to process the items.
  void Finalize();

  int NumSlowPaths() const { return paint_op_buffer_.numSlowPaths(); }
  bool HasNonAAPaint() const { return paint_op_buffer_.HasNonAAPaint(); }

  // This gives the total number of PaintOps.
  size_t op_count() const { return paint_op_buffer_.size(); }
  size_t BytesUsed() const;

  const DiscardableImageMap& discardable_image_map() const {
    return image_map_;
  }
  base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
  TakeDecodingModeMap() {
    return image_map_.TakeDecodingModeMap();
  }

  void EmitTraceSnapshot() const;
  void GenerateDiscardableImagesMetadata();

  gfx::Rect VisualRectForTesting(int index) { return visual_rects_[index]; }

  // Generate a PaintRecord from this DisplayItemList, leaving |this| in
  // an empty state.
  sk_sp<PaintRecord> ReleaseAsRecord();

  // If a rectangle is solid color, returns that color. |max_ops_to_analyze|
  // indicates the maximum number of draw ops we consider when determining if a
  // rectangle is solid color.
  bool GetColorIfSolidInRect(const gfx::Rect& rect,
                             SkColor* color,
                             int max_ops_to_analyze = 1);

 private:
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, AsValueWithNoOps);
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, AsValueWithOps);
  friend gpu::gles2::GLES2Implementation;

  ~DisplayItemList();

  void Reset();

  std::unique_ptr<base::trace_event::TracedValue> CreateTracedValue(
      bool include_items) const;

  // If we're currently within a paired display item block, unions the
  // given visual rect with the begin display item's visual rect.
  void GrowCurrentBeginItemVisualRect(const gfx::Rect& visual_rect);

  // RTree stores indices into the paint op buffer.
  // TODO(vmpstr): Update the rtree to store offsets instead.
  RTree<size_t> rtree_;
  DiscardableImageMap image_map_;
  PaintOpBuffer paint_op_buffer_;

  // The visual rects associated with each of the display items in the
  // display item list. These rects are intentionally kept separate because they
  // are used to decide which ops to walk for raster.
  std::vector<gfx::Rect> visual_rects_;
  // Byte offsets associated with each of the ops.
  std::vector<size_t> offsets_;
  // A stack of pairs of indices and counts. The indices are into the
  // |visual_rects_| for each paired begin range that hasn't been closed. The
  // counts refer to the number of visual rects in that begin sequence that end
  // with the index.
  std::vector<std::pair<size_t, size_t>> begin_paired_indices_;
  // While recording a range of ops, this is the position in the PaintOpBuffer
  // where the recording started.
  size_t current_range_start_ = 0;
  // For debugging, tracks the number of currently nested visual rects being
  // added.
  int in_paired_begin_count_ = 0;
  // For debugging, tracks if we're painting a visual rect range, to prevent
  // nesting.
  bool in_painting_ = false;

  UsageHint usage_hint_;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, BytesUsed);
  DISALLOW_COPY_AND_ASSIGN(DisplayItemList);
};

}  // namespace cc

#endif  // CC_PAINT_DISPLAY_ITEM_LIST_H_
