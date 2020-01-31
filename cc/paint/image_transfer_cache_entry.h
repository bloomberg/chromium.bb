// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "cc/paint/transfer_cache_entry.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVASizeInfo.h"

class GrContext;
class SkColorSpace;
class SkImage;
class SkPixmap;

namespace cc {

static constexpr uint32_t kInvalidImageTransferCacheEntryId =
    static_cast<uint32_t>(-1);

enum class YUVDecodeFormat {
  kYUV3,   // e.g., YUV 4:2:0, 4:2:2, or 4:4:4 as 3 planes.
  kYUVA4,  // e.g., YUV 4:2:0 as 3 planes plus an alpha plane.
  kYVU3,   // e.g., YVU 4:2:0, 4:2:2, or 4:4:4 as 3 planes.
  kYUV2,   // e.g., YUV 4:2:0 as NV12 (2 planes).
  kUnknown,
  kMaxValue = kUnknown,
};

CC_PAINT_EXPORT size_t NumberOfPlanesForYUVDecodeFormat(YUVDecodeFormat format);

// Client/ServiceImageTransferCacheEntry implement a transfer cache entry
// for transferring image data. On the client side, this is a CPU SkPixmap,
// on the service side the image is uploaded and is a GPU SkImage.
class CC_PAINT_EXPORT ClientImageTransferCacheEntry
    : public ClientTransferCacheEntryBase<TransferCacheEntryType::kImage> {
 public:
  explicit ClientImageTransferCacheEntry(const SkPixmap* pixmap,
                                         const SkColorSpace* target_color_space,
                                         bool needs_mips);
  explicit ClientImageTransferCacheEntry(
      const SkPixmap* y_pixmap,
      const SkPixmap* u_pixmap,
      const SkPixmap* v_pixmap,
      const SkColorSpace* decoded_color_space,
      SkYUVColorSpace yuv_color_space,
      bool needs_mips);
  ~ClientImageTransferCacheEntry() final;

  uint32_t Id() const final;

  // ClientTransferCacheEntry implementation:
  uint32_t SerializedSize() const final;
  bool Serialize(base::span<uint8_t> data) const final;

  static uint32_t GetNextId() { return s_next_id_.GetNext(); }
  bool IsYuv() const { return !!yuv_pixmaps_; }

 private:
  const bool needs_mips_ = false;
  const uint32_t num_planes_ = 1;
  uint32_t id_;
  uint32_t size_ = 0;
  static base::AtomicSequenceNumber s_next_id_;

  // RGBX-only members.
  const SkPixmap* const pixmap_;
  const SkColorSpace* const
      target_color_space_;  // Unused for YUV because Skia handles colorspaces
                            // at raster.

  // YUVA-only members.
  base::Optional<std::array<const SkPixmap*, SkYUVASizeInfo::kMaxCount>>
      yuv_pixmaps_;
  const SkColorSpace* const decoded_color_space_;
  SkYUVColorSpace yuv_color_space_;

  // DCHECKs that the appropriate data members are set or not set and have
  // positive size dimensions.
  void ValidateYUVDataBeforeSerializing() const;
};

class CC_PAINT_EXPORT ServiceImageTransferCacheEntry
    : public ServiceTransferCacheEntryBase<TransferCacheEntryType::kImage> {
 public:
  ServiceImageTransferCacheEntry();
  ~ServiceImageTransferCacheEntry() final;

  ServiceImageTransferCacheEntry(ServiceImageTransferCacheEntry&& other);
  ServiceImageTransferCacheEntry& operator=(
      ServiceImageTransferCacheEntry&& other);

  // Populates this entry using the result of a hardware decode. The assumption
  // is that |plane_images| are backed by textures that are in turn backed by a
  // buffer (dmabuf in Chrome OS) containing the planes of the decoded image.
  // |plane_images_format| indicates the planar layout of |plane_images|.
  // |buffer_byte_size| is the size of the buffer. We assume the following:
  //
  // - The backing textures don't have mipmaps. We will generate the mipmaps if
  //   |needs_mips| is true.
  // - The conversion from YUV to RGB will be performed according to
  //   |yuv_color_space|.
  // - The colorspace of the resulting RGB image is sRGB.
  //
  // Returns true if the entry can be built, false otherwise.
  bool BuildFromHardwareDecodedImage(GrContext* context,
                                     std::vector<sk_sp<SkImage>> plane_images,
                                     YUVDecodeFormat plane_images_format,
                                     SkYUVColorSpace yuv_color_space,
                                     size_t buffer_byte_size,
                                     bool needs_mips);

  // ServiceTransferCacheEntry implementation:
  size_t CachedSize() const final;
  bool Deserialize(GrContext* context, base::span<const uint8_t> data) final;

  bool fits_on_gpu() const { return fits_on_gpu_; }
  const std::vector<sk_sp<SkImage>>& plane_images() const {
    return plane_images_;
  }
  const sk_sp<SkImage>& image() const { return image_; }

  // Ensures the cached image has mips.
  void EnsureMips();
  bool has_mips() const { return has_mips_; }

  // Used in tests and for registering each texture for memory dumps.
  const sk_sp<SkImage>& GetPlaneImage(size_t index) const;
  const std::vector<size_t>& GetPlaneCachedSizes() const {
    return plane_sizes_;
  }
  bool is_yuv() const { return !plane_images_.empty(); }
  size_t num_planes() const { return is_yuv() ? plane_images_.size() : 1u; }

 private:
  sk_sp<SkImage> MakeSkImage(const SkPixmap& pixmap,
                             uint32_t width,
                             uint32_t height,
                             sk_sp<SkColorSpace> target_color_space);

  GrContext* context_ = nullptr;
  std::vector<sk_sp<SkImage>> plane_images_;
  YUVDecodeFormat plane_images_format_ = YUVDecodeFormat::kUnknown;
  std::vector<size_t> plane_sizes_;
  sk_sp<SkImage> image_;
  base::Optional<SkYUVColorSpace> yuv_color_space_;
  bool has_mips_ = false;
  size_t size_ = 0;
  bool fits_on_gpu_ = false;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
