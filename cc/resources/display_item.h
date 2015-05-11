// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DISPLAY_ITEM_H_
#define CC_RESOURCES_DISPLAY_ITEM_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/debug/traced_value.h"
#include "ui/gfx/geometry/rect.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT DisplayItem {
 public:
  virtual ~DisplayItem() {}

  void SetNew(bool is_suitable_for_gpu_rasterization,
              int approximate_op_count,
              size_t picture_memory_usage) {
    is_suitable_for_gpu_rasterization_ = is_suitable_for_gpu_rasterization;
    approximate_op_count_ = approximate_op_count;
    picture_memory_usage_ =
        picture_memory_usage + sizeof(bool) + sizeof(int) + sizeof(size_t);
  }

  virtual void Raster(SkCanvas* canvas,
                      SkDrawPictureCallback* callback) const = 0;
  virtual void AsValueInto(base::trace_event::TracedValue* array) const = 0;

  bool is_suitable_for_gpu_rasterization() const {
    return is_suitable_for_gpu_rasterization_;
  }
  int approximate_op_count() const { return approximate_op_count_; }
  size_t picture_memory_usage() const { return picture_memory_usage_; }

 protected:
  DisplayItem();

  bool is_suitable_for_gpu_rasterization_;
  int approximate_op_count_;
  size_t picture_memory_usage_;
};

}  // namespace cc

#endif  // CC_RESOURCES_DISPLAY_ITEM_H_
