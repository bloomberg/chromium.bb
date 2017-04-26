// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_RECORD_H_
#define CC_PAINT_PAINT_RECORD_H_

#include "third_party/skia/include/core/SkPicture.h"

namespace cc {

using PaintRecord = SkPicture;

inline sk_sp<SkPicture> ToSkPicture(sk_sp<PaintRecord> record) {
  return record;
}

inline sk_sp<const SkPicture> ToSkPicture(sk_sp<const PaintRecord> record) {
  return record;
}

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORD_H_
