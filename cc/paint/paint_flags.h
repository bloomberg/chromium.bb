// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_FLAGS_H_
#define CC_PAINT_PAINT_FLAGS_H_

#include "third_party/skia/include/core/SkPaint.h"

namespace cc {

using PaintFlags = SkPaint;

inline const SkPaint& ToSkPaint(const PaintFlags& flags) {
  return flags;
}

}  // namespace cc

#endif  // CC_PAINT_PAINT_FLAGS_H_
