// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DISPLAY_ITEM_LIST_H_
#define CC_RESOURCES_DISPLAY_ITEM_LIST_H_

#include "base/debug/trace_event.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/resources/display_item.h"
#include "ui/gfx/rect.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  static scoped_refptr<DisplayItemList> Create();

  void Raster(SkCanvas* canvas,
              SkDrawPictureCallback* callback,
              float contents_scale) const;

  void AppendItem(scoped_ptr<DisplayItem> item);

  bool IsSuitableForGpuRasterization() const;
  int ApproximateOpCount() const;
  size_t PictureMemoryUsage() const;

  scoped_refptr<base::debug::ConvertableToTraceFormat> AsValue() const;

  void EmitTraceSnapshot() const;

 private:
  DisplayItemList();
  ~DisplayItemList();
  ScopedPtrVector<DisplayItem> items_;
  bool is_suitable_for_gpu_rasterization_;
  int approximate_op_count_;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  DISALLOW_COPY_AND_ASSIGN(DisplayItemList);
};

}  // namespace cc

#endif  // CC_RESOURCES_DISPLAY_ITEM_LIST_H_
