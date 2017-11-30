// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_

#include <vector>

#include "cc/paint/transfer_cache_entry.h"
#include "third_party/skia/include/core/SkImage.h"

namespace cc {

// Client/ServiceImageTransferCacheEntry implement a transfer cache entry
// for transferring image data. On the client side, this is a CPU SkPixmap,
// on the service side the image is uploaded and is a GPU SkImage.
class CC_PAINT_EXPORT ClientImageTransferCacheEntry
    : public ClientTransferCacheEntry {
 public:
  explicit ClientImageTransferCacheEntry(
      const SkPixmap* pixmap,
      const SkColorSpace* target_color_space);
  ~ClientImageTransferCacheEntry() override;

  // ClientTransferCacheEntry implementation:
  TransferCacheEntryType Type() const override;
  size_t SerializedSize() const override;
  bool Serialize(size_t size, uint8_t* data) const override;

 private:
  const SkPixmap* const pixmap_;
  size_t size_ = 0;
};

class CC_PAINT_EXPORT ServiceImageTransferCacheEntry
    : public ServiceTransferCacheEntry {
 public:
  ServiceImageTransferCacheEntry();
  ~ServiceImageTransferCacheEntry() override;

  // ServiceTransferCacheEntry implementation:
  TransferCacheEntryType Type() const override;
  size_t Size() const override;
  bool Deserialize(GrContext* context, size_t size, uint8_t* data) override;

  const sk_sp<SkImage>& image() { return image_; }

 private:
  sk_sp<SkImage> image_;
  size_t size_ = 0;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
