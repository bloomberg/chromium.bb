// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_ETC1_PIXEL_REF_H_
#define CC_RESOURCES_ETC1_PIXEL_REF_H_

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkFlattenable.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace cc {

class CC_EXPORT ETC1PixelRef : public SkPixelRef {
 public:
  // Takes ownership of pixels.
  explicit ETC1PixelRef(scoped_ptr<uint8_t[]> pixels);
  virtual ~ETC1PixelRef();

  // SK_DECLARE_UNFLATTENABLE_OBJECT
  virtual Factory getFactory() const SK_OVERRIDE;

 protected:
  // Implementation of SkPixelRef.
  virtual void* onLockPixels(SkColorTable** color_table) SK_OVERRIDE;
  virtual void onUnlockPixels() SK_OVERRIDE;

 private:
  scoped_ptr<uint8_t[]> pixels_;
};

}  // namespace cc

#endif  // CC_RESOURCES_ETC1_PIXEL_REF_H_
