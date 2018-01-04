// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/decoded_draw_image.h"

namespace cc {

DecodedDrawImage::DecodedDrawImage(sk_sp<const SkImage> image,
                                   const SkSize& src_rect_offset,
                                   const SkSize& scale_adjustment,
                                   SkFilterQuality filter_quality)
    : image_(std::move(image)),
      src_rect_offset_(src_rect_offset),
      scale_adjustment_(scale_adjustment),
      filter_quality_(filter_quality) {}

DecodedDrawImage::DecodedDrawImage(
    base::Optional<uint32_t> transfer_cache_entry_id,
    const SkSize& src_rect_offset,
    const SkSize& scale_adjustment,
    SkFilterQuality filter_quality)
    : transfer_cache_entry_id_(transfer_cache_entry_id),
      src_rect_offset_(src_rect_offset),
      scale_adjustment_(scale_adjustment),
      filter_quality_(filter_quality) {}

DecodedDrawImage::DecodedDrawImage()
    : DecodedDrawImage(nullptr, kNone_SkFilterQuality) {}

DecodedDrawImage::DecodedDrawImage(sk_sp<const SkImage> image,
                                   SkFilterQuality filter_quality)
    : DecodedDrawImage(std::move(image),
                       SkSize::Make(0.f, 0.f),
                       SkSize::Make(1.f, 1.f),
                       filter_quality) {}

DecodedDrawImage::DecodedDrawImage(const DecodedDrawImage& other) = default;

DecodedDrawImage::~DecodedDrawImage() = default;

}  // namespace cc
