// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/etc1_pixel_ref.h"

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace cc {

#ifdef SK_SUPPORT_LEGACY_PIXELREF_CONSTRUCTOR
// Takes ownership of pixels.
ETC1PixelRef::ETC1PixelRef(scoped_ptr<uint8_t[]> pixels)
    : pixels_(pixels.Pass()) {
  setImmutable();
}
#endif

// Takes ownership of pixels.
ETC1PixelRef::ETC1PixelRef(const SkImageInfo& info,
                           scoped_ptr<uint8_t[]> pixels)
    : SkPixelRef(info), pixels_(pixels.Pass()) {
  setImmutable();
}

ETC1PixelRef::~ETC1PixelRef() {}

void* ETC1PixelRef::onLockPixels(SkColorTable** color_table) {
  *color_table = NULL;
  return static_cast<void*>(pixels_.get());
}

void ETC1PixelRef::onUnlockPixels() {}

SkFlattenable::Factory ETC1PixelRef::getFactory() const { return NULL; }

}  // namespace cc
