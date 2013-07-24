// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/copy_output_result.h"

#include "base/logging.h"
#include "cc/resources/texture_mailbox.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

CopyOutputResult::CopyOutputResult() {}

CopyOutputResult::CopyOutputResult(scoped_ptr<SkBitmap> bitmap)
    : size_(bitmap->width(), bitmap->height()),
      bitmap_(bitmap.Pass()) {
  DCHECK(bitmap_);
}

CopyOutputResult::CopyOutputResult(gfx::Size size,
                                   scoped_ptr<TextureMailbox> texture_mailbox)
    : size_(size),
      texture_mailbox_(texture_mailbox.Pass()) {
  DCHECK(texture_mailbox_);
  DCHECK(texture_mailbox_->IsTexture());
}

CopyOutputResult::~CopyOutputResult() {
  if (texture_mailbox_)
    texture_mailbox_->RunReleaseCallback(0, false);
}

scoped_ptr<SkBitmap> CopyOutputResult::TakeBitmap() {
  return bitmap_.Pass();
}

scoped_ptr<TextureMailbox> CopyOutputResult::TakeTexture() {
  return texture_mailbox_.Pass();
}

}  // namespace cc
