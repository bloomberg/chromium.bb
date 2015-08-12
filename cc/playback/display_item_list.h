// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISPLAY_ITEM_LIST_H_
#define CC_PLAYBACK_DISPLAY_ITEM_LIST_H_

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/base/sidecar_list_container.h"
#include "cc/playback/display_item.h"
#include "cc/playback/pixel_ref_map.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/geometry/rect.h"

class SkCanvas;
class SkPictureRecorder;

namespace cc {

class DisplayItemListSettings;

class CC_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  static scoped_refptr<DisplayItemList> CreateWithoutCachedPicture(
      const DisplayItemListSettings& settings);

  // Creates a display item list with the given cull rect (if picture caching
  // is used). The resulting display list will not support sidecar data.
  static scoped_refptr<DisplayItemList> Create(gfx::Rect layer_rect,
                                               bool use_cached_picture);

  static scoped_refptr<DisplayItemList> Create(
      gfx::Rect layer_rect,
      const DisplayItemListSettings& settings);

  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback,
              const gfx::Rect& canvas_target_playback_rect,
              float contents_scale) const;

  // This is a fast path for use only if canvas_ is set and
  // retain_individual_display_items_ is false. This method also updates
  // is_suitable_for_gpu_rasterization_ and approximate_op_count_.
  void RasterIntoCanvas(const DisplayItem& display_item);

  template <typename DisplayItemType>
  DisplayItemType* CreateAndAppendItem() {
#if DCHECK_IS_ON()
    needs_process_ = true;
#endif
    ProcessAppendedItemsOnTheFly();
    return items_.AllocateAndConstruct<DisplayItemType>();
  }

  // Removes the last item. This cannot be called on lists with cached pictures
  // (since the data may already have been incorporated into cached picture
  // sizes, etc).
  void RemoveLast();

  // Called after all items are appended, to process the items and, if
  // applicable, create an internally cached SkPicture.
  void Finalize();

  bool IsSuitableForGpuRasterization() const;
  int ApproximateOpCount() const;
  size_t ApproximateMemoryUsage() const;
  bool ShouldBeAnalyzedForSolidColor() const;

  bool RetainsIndividualDisplayItems() const;

  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValue(
      bool include_items) const;

  void EmitTraceSnapshot() const;

  void GatherPixelRefs(const gfx::Size& grid_cell_size);

  // Finds the sidecar for a display item in this list.
  void* GetSidecar(DisplayItem* display_item);

 private:
  DisplayItemList(gfx::Rect layer_rect,
                  const DisplayItemListSettings& display_list_settings,
                  bool retain_individual_display_items);
  DisplayItemList(gfx::Rect layer_rect,
                  const DisplayItemListSettings& display_list_settings);
  ~DisplayItemList();

  // While appending new items, if they are not being retained, this can process
  // periodically to avoid retaining all the items and processing at the end.
  void ProcessAppendedItemsOnTheFly();
  void ProcessAppendedItems();
#if DCHECK_IS_ON()
  bool ProcessAppendedItemsCalled() const { return !needs_process_; }
  bool needs_process_;
#else
  bool ProcessAppendedItemsCalled() const { return true; }
#endif

  SidecarListContainer<DisplayItem> items_;
  skia::RefPtr<SkPicture> picture_;

  scoped_ptr<SkPictureRecorder> recorder_;
  skia::RefPtr<SkCanvas> canvas_;
  bool use_cached_picture_;
  bool retain_individual_display_items_;

  gfx::Rect layer_rect_;
  bool all_items_are_suitable_for_gpu_rasterization_;
  int approximate_op_count_;

  // Memory usage due to the cached SkPicture.
  size_t picture_memory_usage_;

  // Memory usage due to external data held by display items.
  size_t external_memory_usage_;

  scoped_ptr<PixelRefMap> pixel_refs_;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  friend class PixelRefMap::Iterator;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, ApproximateMemoryUsage);
  DISALLOW_COPY_AND_ASSIGN(DisplayItemList);
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_ITEM_LIST_H_
