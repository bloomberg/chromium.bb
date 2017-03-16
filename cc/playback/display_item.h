// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISPLAY_ITEM_H_
#define CC_PLAYBACK_DISPLAY_ITEM_H_

#include <stddef.h>

#include <memory>

#include "cc/base/cc_export.h"
#include "cc/debug/traced_value.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class CC_EXPORT DisplayItem {
 public:
  virtual ~DisplayItem() = default;

  enum Type {
    CLIP,
    END_CLIP,
    CLIP_PATH,
    END_CLIP_PATH,
    COMPOSITING,
    END_COMPOSITING,
    DRAWING,
    FILTER,
    END_FILTER,
    FLOAT_CLIP,
    END_FLOAT_CLIP,
    TRANSFORM,
    END_TRANSFORM,
  };
  const Type type;

 protected:
  explicit DisplayItem(Type type) : type(type) {}
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_ITEM_H_
