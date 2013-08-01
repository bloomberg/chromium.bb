// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/ui_resource_bitmap.h"

#include "base/memory/scoped_ptr.h"

namespace cc {

scoped_refptr<UIResourceBitmap>
UIResourceBitmap::Create(uint8_t* pixels,
                         UIResourceFormat format,
                         gfx::Size size) {
  scoped_refptr<UIResourceBitmap> ret = new UIResourceBitmap();
  ret->pixels_ = scoped_ptr<uint8_t[]>(pixels);
  ret->format_ = format;
  ret->size_ = size;

  return ret;
}

UIResourceBitmap::UIResourceBitmap() {}
UIResourceBitmap::~UIResourceBitmap() {}

}  // namespace cc
