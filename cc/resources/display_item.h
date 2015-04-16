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

  virtual void Raster(SkCanvas* canvas,
                      SkDrawPictureCallback* callback) const = 0;
  virtual void RasterForTracing(SkCanvas* canvas) const;

  virtual bool IsSuitableForGpuRasterization() const = 0;
  virtual int ApproximateOpCount() const = 0;
  virtual size_t PictureMemoryUsage() const = 0;
  virtual void AsValueInto(base::trace_event::TracedValue* array) const = 0;

 protected:
  DisplayItem();
};

}  // namespace cc

#endif  // CC_RESOURCES_DISPLAY_ITEM_H_
