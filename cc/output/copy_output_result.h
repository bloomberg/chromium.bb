// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COPY_OUTPUT_RESULT_H_
#define CC_OUTPUT_COPY_OUTPUT_RESULT_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/base/cc_export.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

class SkBitmap;

namespace cc {

namespace mojom {
class CopyOutputResultDataView;
}

class TextureMailbox;

class CC_EXPORT CopyOutputResult {
 public:
  static std::unique_ptr<CopyOutputResult> CreateEmptyResult() {
    return base::WrapUnique(new CopyOutputResult);
  }
  static std::unique_ptr<CopyOutputResult> CreateBitmapResult(
      std::unique_ptr<SkBitmap> bitmap) {
    return base::WrapUnique(new CopyOutputResult(std::move(bitmap)));
  }
  static std::unique_ptr<CopyOutputResult> CreateTextureResult(
      const gfx::Size& size,
      const TextureMailbox& texture_mailbox,
      std::unique_ptr<SingleReleaseCallback> release_callback) {
    return base::WrapUnique(new CopyOutputResult(size, texture_mailbox,
                                                 std::move(release_callback)));
  }

  ~CopyOutputResult();

  bool IsEmpty() const { return !HasBitmap() && !HasTexture(); }
  bool HasBitmap() const { return !!bitmap_ && !bitmap_->isNull(); }
  bool HasTexture() const { return texture_mailbox_.IsValid(); }

  gfx::Size size() const { return size_; }
  std::unique_ptr<SkBitmap> TakeBitmap();
  void TakeTexture(TextureMailbox* texture_mailbox,
                   std::unique_ptr<SingleReleaseCallback>* release_callback);

 private:
  friend struct mojo::StructTraits<mojom::CopyOutputResultDataView,
                                   std::unique_ptr<CopyOutputResult>>;

  CopyOutputResult();
  explicit CopyOutputResult(std::unique_ptr<SkBitmap> bitmap);
  explicit CopyOutputResult(
      const gfx::Size& size,
      const TextureMailbox& texture_mailbox,
      std::unique_ptr<SingleReleaseCallback> release_callback);

  gfx::Size size_;
  std::unique_ptr<SkBitmap> bitmap_;
  TextureMailbox texture_mailbox_;
  std::unique_ptr<SingleReleaseCallback> release_callback_;

  DISALLOW_COPY_AND_ASSIGN(CopyOutputResult);
};

}  // namespace cc

#endif  // CC_OUTPUT_COPY_OUTPUT_RESULT_H_
