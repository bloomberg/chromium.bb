// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_RECORD_H_
#define CC_PAINT_PAINT_RECORD_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"

namespace cc {

class PaintCanvas;

// This class is only used by casting from an SkPicture in PaintRecorder.
// This will be replaced with its own functionality in a future patch.
class CC_PAINT_EXPORT PaintRecord : private SkPicture {
 public:
  void playback(PaintCanvas* canvas);

  using SkPicture::playback;
  using SkPicture::approximateBytesUsed;
  using SkPicture::approximateOpCount;
  using SkPicture::cullRect;
  using SkRefCnt::ref;
  using SkRefCnt::unref;

 private:
  friend class PaintRecorder;
  friend const SkPicture* ToSkPicture(const PaintRecord* record);
  friend SkPicture* ToSkPicture(PaintRecord* record);
  friend sk_sp<SkPicture> ToSkPicture(sk_sp<PaintRecord> record);
  friend sk_sp<const SkPicture> ToSkPicture(sk_sp<const PaintRecord> record);

  DISALLOW_COPY_AND_ASSIGN(PaintRecord);
};

ALWAYS_INLINE const SkPicture* ToSkPicture(const PaintRecord* record) {
  return static_cast<const SkPicture*>(record);
}

ALWAYS_INLINE SkPicture* ToSkPicture(PaintRecord* record) {
  return static_cast<SkPicture*>(record);
}

ALWAYS_INLINE sk_sp<SkPicture> ToSkPicture(sk_sp<PaintRecord> record) {
  return sk_ref_sp(ToSkPicture(record.get()));
}

ALWAYS_INLINE sk_sp<const SkPicture> ToSkPicture(
    sk_sp<const PaintRecord> record) {
  return sk_ref_sp(ToSkPicture(record.get()));
}

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORD_H_
