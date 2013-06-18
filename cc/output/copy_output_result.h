// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COPY_OUTPUT_RESULT_H_
#define CC_OUTPUT_COPY_OUTPUT_RESULT_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace cc {
class TextureMailbox;

class CC_EXPORT CopyOutputResult {
 public:
  static scoped_ptr<CopyOutputResult> CreateEmptyResult() {
    return make_scoped_ptr(new CopyOutputResult);
  }
  static scoped_ptr<CopyOutputResult> CreateBitmapResult(
      scoped_ptr<SkBitmap> bitmap) {
    return make_scoped_ptr(new CopyOutputResult(bitmap.Pass()));
  }
  static scoped_ptr<CopyOutputResult> CreateTextureResult(
      gfx::Size size,
      scoped_ptr<TextureMailbox> texture_mailbox) {
    return make_scoped_ptr(new CopyOutputResult(size, texture_mailbox.Pass()));
  }

  ~CopyOutputResult();

  bool IsEmpty() const { return !HasBitmap() && !HasTexture(); }
  bool HasBitmap() const { return !!bitmap_; }
  bool HasTexture() const { return !!texture_mailbox_; }

  gfx::Size size() const { return size_; }
  scoped_ptr<SkBitmap> TakeBitmap();
  scoped_ptr<TextureMailbox> TakeTexture();

 private:
  CopyOutputResult();
  explicit CopyOutputResult(scoped_ptr<SkBitmap> bitmap);
  explicit CopyOutputResult(gfx::Size size,
                            scoped_ptr<TextureMailbox> texture_mailbox);

  gfx::Size size_;
  scoped_ptr<SkBitmap> bitmap_;
  scoped_ptr<TextureMailbox> texture_mailbox_;
};

}  // namespace cc

#endif  // CC_OUTPUT_COPY_OUTPUT_RESULT_H_
