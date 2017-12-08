// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SCOPED_RASTER_FLAGS_H_
#define CC_PAINT_SCOPED_RASTER_FLAGS_H_

#include "base/containers/stack_container.h"
#include "base/macros.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"

namespace cc {

// A helper class to modify the flags for raster. This includes alpha folding
// from SaveLayers and decoding images.
class CC_PAINT_EXPORT ScopedRasterFlags {
 public:
  // |flags| and |image_provider| must outlive this class.
  ScopedRasterFlags(const PaintFlags* flags,
                    ImageProvider* image_provider,
                    const SkMatrix& ctm,
                    uint8_t alpha);
  ~ScopedRasterFlags();

  // The usage of these flags should not extend beyond the lifetime of this
  // object.
  const PaintFlags* flags() const {
    if (decode_failed_)
      return nullptr;

    return modified_flags_ ? &*modified_flags_ : original_flags_;
  }

 private:
  // An ImageProvider that passes decode requests through to the
  // |source_provider| but keeps the decode cached throughtout its lifetime,
  // instead of passing the ref to the caller.
  class DecodeStashingImageProvider : public ImageProvider {
   public:
    // |source_provider| must outlive this class.
    explicit DecodeStashingImageProvider(ImageProvider* source_provider);
    ~DecodeStashingImageProvider() override;

    // ImageProvider implementation.
    ScopedDecodedDrawImage GetDecodedDrawImage(
        const DrawImage& draw_image) override;

   private:
    ImageProvider* source_provider_;
    base::StackVector<ScopedDecodedDrawImage, 1> decoded_images_;

    DISALLOW_COPY_AND_ASSIGN(DecodeStashingImageProvider);
  };

  void DecodeImageShader(const SkMatrix& ctm);
  void DecodeRecordShader(const SkMatrix& ctm);
  void AdjustStrokeIfNeeded(const SkMatrix& ctm);

  PaintFlags* MutableFlags() {
    if (!modified_flags_)
      modified_flags_.emplace(*original_flags_);
    return &*modified_flags_;
  }

  const PaintFlags* original_flags_;
  base::Optional<PaintFlags> modified_flags_;
  base::Optional<DecodeStashingImageProvider> decode_stashing_image_provider_;
  bool decode_failed_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedRasterFlags);
};

}  // namespace cc

#endif  // CC_PAINT_SCOPED_RASTER_FLAGS_H_
