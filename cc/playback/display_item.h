// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISPLAY_ITEM_H_
#define CC_PLAYBACK_DISPLAY_ITEM_H_

#include <stddef.h>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/debug/traced_value.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/geometry/rect.h"

class SkCanvas;

namespace cc {
class ImageSerializationProcessor;

namespace proto {
class DisplayItem;
}  // namespace proto

class CC_EXPORT DisplayItem {
 public:
  virtual ~DisplayItem() {}

  virtual void ToProtobuf(
      proto::DisplayItem* proto,
      ImageSerializationProcessor* image_serialization_processor) const = 0;
  virtual void Raster(SkCanvas* canvas,
                      const gfx::Rect& canvas_target_playback_rect,
                      SkPicture::AbortCallback* callback) const = 0;
  virtual void AsValueInto(const gfx::Rect& visual_rect,
                           base::trace_event::TracedValue* array) const = 0;
  // For tracing.
  virtual size_t ExternalMemoryUsage() const = 0;

 protected:
  DisplayItem();
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_ITEM_H_
