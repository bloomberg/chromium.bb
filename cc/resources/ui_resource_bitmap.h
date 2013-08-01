// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_UI_RESOURCE_BITMAP_H_
#define CC_RESOURCES_UI_RESOURCE_BITMAP_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/gfx/size.h"

namespace cc {

// Ref-counted bitmap class (can’t use SkBitmap because of ETC1).  Thread-safety
// ensures that both main and impl threads can hold references to the bitmap and
// that asynchronous uploads are allowed.
class CC_EXPORT UIResourceBitmap
    : public base::RefCountedThreadSafe<UIResourceBitmap> {
 public:
  enum UIResourceFormat {
    RGBA8
  };

  // Takes ownership of “pixels”.
  static scoped_refptr<UIResourceBitmap> Create(uint8_t* pixels,
                                                UIResourceFormat format,
                                                gfx::Size size);

  gfx::Size GetSize() const { return size_; }
  UIResourceFormat GetFormat() const { return format_; }
  uint8_t* GetPixels() { return pixels_.get(); }

 private:
  friend class base::RefCountedThreadSafe<UIResourceBitmap>;

  UIResourceBitmap();
  ~UIResourceBitmap();

  scoped_ptr<uint8_t[]> pixels_;
  UIResourceFormat format_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(UIResourceBitmap);
};

}  // namespace cc

#endif  // CC_RESOURCES_UI_RESOURCE_BITMAP_H_
