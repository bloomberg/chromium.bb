// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/image_transfer_cache_entry.h"

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const SkPixmap* pixmap,
    const SkColorSpace* target_color_space)
    : id_(s_next_id_.GetNext()), pixmap_(pixmap) {
  // Compute and cache the size of the data.
  // We write the following:
  // - Image color type (uint32_t)
  // - Image width (uint32_t)
  // - Image height (uint32_t)
  // - Pixels size (uint32_t)
  // - Pixels (variable)
  base::CheckedNumeric<size_t> safe_size;
  safe_size += sizeof(uint32_t);  // color type
  safe_size += sizeof(uint32_t);  // width
  safe_size += sizeof(uint32_t);  // height
  safe_size += sizeof(size_t);    // pixels size
  safe_size += pixmap_->computeByteSize();
  safe_size += PaintOpWriter::HeaderBytes();
  size_ = safe_size.ValueOrDie();
  // TODO(ericrk): Handle colorspace.
}
ClientImageTransferCacheEntry::~ClientImageTransferCacheEntry() = default;

// static
base::AtomicSequenceNumber ClientImageTransferCacheEntry::s_next_id_;

size_t ClientImageTransferCacheEntry::SerializedSize() const {
  return size_;
}

uint32_t ClientImageTransferCacheEntry::Id() const {
  return id_;
}

bool ClientImageTransferCacheEntry::Serialize(base::span<uint8_t> data) const {
  DCHECK_GE(data.size(), SerializedSize());

  PaintOpWriter writer(data.data(), data.size(), nullptr, nullptr);
  writer.Write(pixmap_->colorType());
  writer.Write(pixmap_->width());
  writer.Write(pixmap_->height());
  size_t pixmap_size = pixmap_->computeByteSize();
  writer.WriteSize(pixmap_size);
  writer.WriteData(pixmap_size, pixmap_->addr());
  // TODO(ericrk): Handle colorspace.

  if (writer.size() != data.size())
    return false;

  return true;
}

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry() = default;
ServiceImageTransferCacheEntry::~ServiceImageTransferCacheEntry() = default;

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry(
    ServiceImageTransferCacheEntry&& other) = default;
ServiceImageTransferCacheEntry& ServiceImageTransferCacheEntry::operator=(
    ServiceImageTransferCacheEntry&& other) = default;

size_t ServiceImageTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServiceImageTransferCacheEntry::Deserialize(GrContext* context,
                                                 base::span<uint8_t> data) {
  PaintOpReader reader(data.data(), data.size(), nullptr);
  SkColorType color_type;
  reader.Read(&color_type);
  uint32_t width;
  reader.Read(&width);
  uint32_t height;
  reader.Read(&height);
  size_t pixel_size;
  reader.ReadSize(&pixel_size);
  size_ = data.size();
  if (!reader.valid())
    return false;
  // TODO(ericrk): Handle colorspace.

  SkImageInfo image_info =
      SkImageInfo::Make(width, height, color_type, kPremul_SkAlphaType);
  const volatile void* pixel_data = reader.ExtractReadableMemory(pixel_size);
  if (!reader.valid())
    return false;

  // Const-cast away the "volatile" on |pixel_data|. We specifically understand
  // that a malicious caller may change our pixels under us, and are OK with
  // this as the worst case scenario is visual corruption.
  SkPixmap pixmap(image_info, const_cast<const void*>(pixel_data),
                  image_info.minRowBytes());

  // Depending on whether the pixmap will fit in a GPU texture, either create
  // a software or GPU SkImage.
  uint32_t max_size = context->caps()->maxTextureSize();
  bool fits_on_gpu = width <= max_size && height <= max_size;
  if (fits_on_gpu) {
    sk_sp<SkImage> image = SkImage::MakeFromRaster(pixmap, nullptr, nullptr);
    DCHECK(image);
    image_ = image->makeTextureImage(context, nullptr);
  } else {
    image_ = SkImage::MakeRasterCopy(pixmap);
  }

  return image_;
}

}  // namespace cc
