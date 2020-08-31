// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/image_transfer_cache_entry.h"

#include <array>
#include <type_traits>
#include <utility>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace cc {
namespace {

// Creates a SkImage backed by the YUV textures corresponding to |plane_images|.
// The layout is specified by |plane_images_format|). The backend textures are
// first extracted out of the |plane_images| (and work is flushed on each one).
// Note that we assume that the image is opaque (no alpha plane). Then, a
// SkImage is created out of those textures using the
// SkImage::MakeFromYUVATextures() API. Finally, |image_color_space| is the
// color space of the resulting image after applying |yuv_color_space|
// (converting from YUV to RGB). This is assumed to be sRGB if nullptr.
//
// On success, the resulting SkImage is
// returned. On failure, nullptr is returned (e.g., if one of the backend
// textures is invalid or a Skia error occurs).
sk_sp<SkImage> MakeYUVImageFromUploadedPlanes(
    GrContext* context,
    const std::vector<sk_sp<SkImage>>& plane_images,
    YUVDecodeFormat plane_images_format,
    SkYUVColorSpace yuv_color_space,
    sk_sp<SkColorSpace> image_color_space) {
  // 1) Extract the textures.
  DCHECK_NE(YUVDecodeFormat::kUnknown, plane_images_format);
  DCHECK_EQ(NumberOfPlanesForYUVDecodeFormat(plane_images_format),
            plane_images.size());
  DCHECK_LE(plane_images.size(),
            base::checked_cast<size_t>(SkYUVASizeInfo::kMaxCount));
  std::array<GrBackendTexture, SkYUVASizeInfo::kMaxCount>
      plane_backend_textures;
  for (size_t plane = 0u; plane < plane_images.size(); plane++) {
    plane_backend_textures[plane] = plane_images[plane]->getBackendTexture(
        true /* flushPendingGrContextIO */);
    if (!plane_backend_textures[plane].isValid()) {
      DLOG(ERROR) << "Invalid backend texture found";
      return nullptr;
    }
  }

  // 2) Create the YUV image.
  SkYUVAIndex plane_indices[SkYUVAIndex::kIndexCount];
  if (plane_images_format == YUVDecodeFormat::kYUV3) {
    plane_indices[SkYUVAIndex::kY_Index] = {0, SkColorChannel::kR};
    plane_indices[SkYUVAIndex::kU_Index] = {1, SkColorChannel::kR};
    plane_indices[SkYUVAIndex::kV_Index] = {2, SkColorChannel::kR};
  } else if (plane_images_format == YUVDecodeFormat::kYVU3) {
    plane_indices[SkYUVAIndex::kY_Index] = {0, SkColorChannel::kR};
    plane_indices[SkYUVAIndex::kU_Index] = {2, SkColorChannel::kR};
    plane_indices[SkYUVAIndex::kV_Index] = {1, SkColorChannel::kR};
  } else if (plane_images_format == YUVDecodeFormat::kYUV2) {
    plane_indices[SkYUVAIndex::kY_Index] = {0, SkColorChannel::kR};
    plane_indices[SkYUVAIndex::kU_Index] = {1, SkColorChannel::kR};
    plane_indices[SkYUVAIndex::kV_Index] = {1, SkColorChannel::kG};
  } else {
    // TODO(crbug.com/910276): handle and test non-opaque images.
    NOTREACHED();
    DLOG(ERROR) << "Unsupported planar format";
    return nullptr;
  }
  plane_indices[SkYUVAIndex::kA_Index] = {-1, SkColorChannel::kR};
  sk_sp<SkImage> image = SkImage::MakeFromYUVATextures(
      context, yuv_color_space, plane_backend_textures.data(), plane_indices,
      plane_images[0]->dimensions(), kTopLeft_GrSurfaceOrigin,
      std::move(image_color_space));
  if (!image) {
    DLOG(ERROR) << "Could not create YUV image";
    return nullptr;
  }

  return image;
}

// TODO(ericrk): Replace calls to this with calls to SkImage::makeTextureImage,
// once that function handles colorspaces. https://crbug.com/834837
sk_sp<SkImage> MakeTextureImage(GrContext* context,
                                sk_sp<SkImage> source_image,
                                sk_sp<SkColorSpace> target_color_space,
                                GrMipMapped mip_mapped) {
  // Step 1: Upload image and generate mips if necessary. If we will be applying
  // a color-space conversion, don't generate mips yet, instead do it after
  // conversion, in step 3.
  // NOTE: |target_color_space| is only passed over the transfer cache if needed
  // (non-null, different from the source color space).
  bool add_mips_after_color_conversion =
      target_color_space && mip_mapped == GrMipMapped::kYes;
  sk_sp<SkImage> uploaded_image = source_image->makeTextureImage(
      context, add_mips_after_color_conversion ? GrMipMapped::kNo : mip_mapped,
      SkBudgeted::kNo);

  // Step 2: Apply a color-space conversion if necessary.
  if (uploaded_image && target_color_space) {
    uploaded_image = uploaded_image->makeColorSpace(target_color_space);
  }

  // Step 3: If we had a colorspace conversion, we couldn't mipmap in step 1, so
  // add mips here.
  if (uploaded_image && add_mips_after_color_conversion) {
    uploaded_image = uploaded_image->makeTextureImage(
        context, GrMipMapped::kYes, SkBudgeted::kNo);
  }

  return uploaded_image;
}

}  // namespace

size_t NumberOfPlanesForYUVDecodeFormat(YUVDecodeFormat format) {
  switch (format) {
    case YUVDecodeFormat::kYUVA4:
      return 4u;
    case YUVDecodeFormat::kYUV3:
    case YUVDecodeFormat::kYVU3:
      return 3u;
    case YUVDecodeFormat::kYUV2:
      return 2u;
    case YUVDecodeFormat::kUnknown:
      return 0u;
  }
}

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const SkPixmap* pixmap,
    const SkColorSpace* target_color_space,
    bool needs_mips)
    : needs_mips_(needs_mips),
      id_(GetNextId()),
      pixmap_(pixmap),
      target_color_space_(target_color_space),
      decoded_color_space_(nullptr) {
  size_t target_color_space_size =
      target_color_space ? target_color_space->writeToMemory(nullptr) : 0u;
  size_t pixmap_color_space_size =
      pixmap_->colorSpace() ? pixmap_->colorSpace()->writeToMemory(nullptr)
                            : 0u;

  // x64 has 8-byte alignment for uint64_t even though x86 has 4-byte
  // alignment.  Always use 8 byte alignment.
  const size_t align = sizeof(uint64_t);

  // Compute and cache the size of the data.
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::HeaderBytes();
  safe_size += sizeof(uint32_t);  // is_yuv
  safe_size += sizeof(uint32_t);  // color type
  safe_size += sizeof(uint32_t);  // width
  safe_size += sizeof(uint32_t);  // height
  safe_size += sizeof(uint32_t);  // has mips
  safe_size += sizeof(uint64_t) + align;  // pixels size + alignment
  safe_size += sizeof(uint64_t) + align;  // row bytes + alignment
  safe_size += target_color_space_size + sizeof(uint64_t) + align;
  safe_size += pixmap_color_space_size + sizeof(uint64_t) + align;
  // Include 4 bytes of padding so we can always align our data pointer to a
  // 4-byte boundary.
  safe_size += 4;
  safe_size += pixmap_->computeByteSize();
  size_ = safe_size.ValueOrDie();
}

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const SkPixmap* y_pixmap,
    const SkPixmap* u_pixmap,
    const SkPixmap* v_pixmap,
    const SkColorSpace* decoded_color_space,
    SkYUVColorSpace yuv_color_space,
    bool needs_mips)
    : needs_mips_(needs_mips),
      num_planes_(3),
      id_(GetNextId()),
      pixmap_(nullptr),
      target_color_space_(nullptr),
      yuv_pixmaps_({y_pixmap, u_pixmap, v_pixmap, nullptr}),
      decoded_color_space_(decoded_color_space),
      yuv_color_space_(yuv_color_space) {
  DCHECK(IsYuv());
  size_t decoded_color_space_size =
      decoded_color_space ? decoded_color_space->writeToMemory(nullptr) : 0u;

  // x64 has 8-byte alignment for uint64_t even though x86 has 4-byte
  // alignment.  Always use 8 byte alignment.
  const size_t align = sizeof(uint64_t);

  // Compute and cache the size of the data.
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::HeaderBytes();
  safe_size += sizeof(uint32_t);  // is_yuv
  safe_size += sizeof(uint32_t);  // num_planes
  safe_size += sizeof(uint32_t);  // has mips
  safe_size += sizeof(uint32_t);  // yuv_color_space
  safe_size += decoded_color_space_size + align;
  safe_size += num_planes_ * sizeof(uint64_t);  // plane widths
  safe_size += num_planes_ * sizeof(uint64_t);  // plane heights
  safe_size += num_planes_ * sizeof(uint64_t);  // plane strides
  safe_size +=
      num_planes_ * (sizeof(uint64_t) + align);  // pixels size + alignment
  // Include 4 bytes of padding before each plane data chunk so we can always
  // align our data pointer to a 4-byte boundary.
  safe_size += 4 * num_planes_;
  safe_size += y_pixmap->computeByteSize();
  safe_size += u_pixmap->computeByteSize();
  safe_size += v_pixmap->computeByteSize();
  size_ = safe_size.ValueOrDie();
}

ClientImageTransferCacheEntry::~ClientImageTransferCacheEntry() = default;

// static
base::AtomicSequenceNumber ClientImageTransferCacheEntry::s_next_id_;

uint32_t ClientImageTransferCacheEntry::SerializedSize() const {
  return size_;
}

uint32_t ClientImageTransferCacheEntry::Id() const {
  return id_;
}

void ClientImageTransferCacheEntry::ValidateYUVDataBeforeSerializing() const {
  DCHECK(!pixmap_);
  DCHECK_LE(yuv_pixmaps_->size(),
            static_cast<uint32_t>(SkYUVASizeInfo::kMaxCount));
  for (uint32_t i = 0; i < num_planes_; ++i) {
    DCHECK(yuv_pixmaps_->at(i));
    const SkPixmap* plane = yuv_pixmaps_->at(i);
    DCHECK_GT(plane->width(), 0);
    DCHECK_GT(plane->height(), 0);
    DCHECK_GT(plane->rowBytes(), 0u);
  }
}

bool ClientImageTransferCacheEntry::Serialize(base::span<uint8_t> data) const {
  DCHECK_GE(data.size(), SerializedSize());
  // We don't need to populate the SerializeOptions here since the writer is
  // only used for serializing primitives.
  PaintOp::SerializeOptions options(nullptr, nullptr, nullptr, nullptr, nullptr,
                                    nullptr, false, false, 0, SkMatrix::I());
  PaintOpWriter writer(data.data(), data.size(), options);
  writer.Write(static_cast<uint32_t>(IsYuv() ? 1 : 0));

  if (IsYuv()) {
    ValidateYUVDataBeforeSerializing();
    writer.Write(num_planes_);
    writer.Write(static_cast<uint32_t>(needs_mips_ ? 1 : 0));
    writer.Write(yuv_color_space_);
    writer.Write(decoded_color_space_);
    for (uint32_t i = 0; i < num_planes_; ++i) {
      DCHECK(yuv_pixmaps_->at(i));
      const SkPixmap* plane = yuv_pixmaps_->at(i);
      writer.Write(plane->width());
      writer.Write(plane->height());
      writer.WriteSize(plane->rowBytes());
      size_t plane_size = plane->computeByteSize();
      if (plane_size == SIZE_MAX)
        return false;
      writer.WriteSize(plane_size);
      writer.AlignMemory(4);
      writer.WriteData(plane_size, plane->addr());
    }

    // Size can't be 0 after serialization unless the writer has become invalid.
    if (writer.size() == 0u)
      return false;

    return true;
  }

  DCHECK_GT(pixmap_->width(), 0);
  DCHECK_GT(pixmap_->height(), 0);

  writer.Write(pixmap_->colorType());
  writer.Write(pixmap_->width());
  writer.Write(pixmap_->height());
  writer.Write(static_cast<uint32_t>(needs_mips_ ? 1 : 0));

  size_t pixmap_size = pixmap_->computeByteSize();
  if (pixmap_size == SIZE_MAX)
    return false;
  writer.WriteSize(pixmap_size);
  writer.WriteSize(pixmap_->rowBytes());
  writer.Write(pixmap_->colorSpace());
  writer.Write(target_color_space_);
  writer.AlignMemory(4);
  writer.WriteData(pixmap_size, pixmap_->addr());

  // Size can't be 0 after serialization unless the writer has become invalid.
  if (writer.size() == 0u)
    return false;

  return true;
}

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry() = default;
ServiceImageTransferCacheEntry::~ServiceImageTransferCacheEntry() = default;

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry(
    ServiceImageTransferCacheEntry&& other) = default;
ServiceImageTransferCacheEntry& ServiceImageTransferCacheEntry::operator=(
    ServiceImageTransferCacheEntry&& other) = default;

bool ServiceImageTransferCacheEntry::BuildFromHardwareDecodedImage(
    GrContext* context,
    std::vector<sk_sp<SkImage>> plane_images,
    YUVDecodeFormat plane_images_format,
    SkYUVColorSpace yuv_color_space,
    size_t buffer_byte_size,
    bool needs_mips) {
  context_ = context;
  size_ = buffer_byte_size;

  // 1) Generate mipmap chains if requested.
  if (needs_mips) {
    DCHECK(plane_sizes_.empty());
    base::CheckedNumeric<size_t> safe_total_size(0u);
    for (size_t plane = 0; plane < plane_images.size(); plane++) {
      plane_images[plane] = plane_images[plane]->makeTextureImage(
          context_, GrMipMapped::kYes, SkBudgeted::kNo);
      if (!plane_images[plane]) {
        DLOG(ERROR) << "Could not generate mipmap chain for plane " << plane;
        return false;
      }
      plane_sizes_.push_back(
          GrContext::ComputeImageSize(plane_images[plane], GrMipMapped::kYes));
      safe_total_size += plane_sizes_.back();
    }
    if (!safe_total_size.AssignIfValid(&size_)) {
      DLOG(ERROR) << "Could not calculate the total image size";
      return false;
    }
  }
  plane_images_ = std::move(plane_images);
  plane_images_format_ = plane_images_format;
  yuv_color_space_ = yuv_color_space;

  // 2) Create a SkImage backed by |plane_images|.
  // TODO(andrescj): support embedded color profiles for hardware decodes and
  // pass the color space to MakeYUVImageFromUploadedPlanes.
  image_ = MakeYUVImageFromUploadedPlanes(context_, plane_images_,
                                          plane_images_format_, yuv_color_space,
                                          SkColorSpace::MakeSRGB());
  if (!image_)
    return false;

  // 3) Fill out the rest of the information.
  has_mips_ = needs_mips;
  fits_on_gpu_ = true;
  return true;
}

size_t ServiceImageTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServiceImageTransferCacheEntry::Deserialize(
    GrContext* context,
    base::span<const uint8_t> data) {
  context_ = context;

  // We don't need to populate the DeSerializeOptions here since the reader is
  // only used for de-serializing primitives.
  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions options(nullptr, nullptr, nullptr,
                                      &scratch_buffer, false);
  PaintOpReader reader(data.data(), data.size(), options);
  uint32_t image_is_yuv = 0;
  reader.Read(&image_is_yuv);
  if (!!image_is_yuv) {
    uint32_t num_planes = 0;
    reader.Read(&num_planes);
    // TODO(crbug.com/910276): Allow for four planes if YUVA.
    // TODO(crbug.com/986575): consider serializing a YUVDecodeFormat.
    if (num_planes != 3u)
      return false;
    plane_images_format_ =
        num_planes == 3u ? YUVDecodeFormat::kYUV3 : YUVDecodeFormat::kYUVA4;
    uint32_t needs_mips;
    reader.Read(&needs_mips);
    has_mips_ = needs_mips;
    SkYUVColorSpace yuv_color_space;
    reader.Read(&yuv_color_space);
    yuv_color_space_ = yuv_color_space;
    sk_sp<SkColorSpace> decoded_color_space;
    reader.Read(&decoded_color_space);

    // Match GrTexture::onGpuMemorySize so that memory traces agree.
    auto gr_mips = has_mips_ ? GrMipMapped::kYes : GrMipMapped::kNo;
    // Read in each plane and reconstruct pixmaps.
    for (uint32_t i = 0; i < num_planes; i++) {
      uint32_t plane_width = 0;
      reader.Read(&plane_width);
      uint32_t plane_height = 0;
      reader.Read(&plane_height);
      size_t plane_stride = 0;
      reader.ReadSize(&plane_stride);
      // Because Skia does not support YUV rasterization from software planes,
      // we require that each pixmap fits in a GPU texture. In the
      // GpuImageDecodeCache, we veto YUV decoding if the planes would be too
      // big.
      uint32_t max_size = static_cast<uint32_t>(context_->maxTextureSize());
      // We compute this for each plane in case a malicious renderer tries to
      // send very large U or V planes.
      fits_on_gpu_ = plane_stride <= max_size && plane_width <= max_size &&
                     plane_height <= max_size;
      if (!fits_on_gpu_ || plane_width == 0 || plane_height == 0 ||
          plane_stride == 0)
        return false;

      size_t plane_bytes;
      reader.ReadSize(&plane_bytes);
      constexpr SkColorType yuv_plane_color_type = kGray_8_SkColorType;
      SkImageInfo plane_pixmap_info =
          SkImageInfo::Make(plane_width, plane_height, yuv_plane_color_type,
                            kPremul_SkAlphaType, decoded_color_space);
      if (plane_pixmap_info.computeMinByteSize() > plane_bytes)
        return false;
      // Align data to a 4-byte boundary, to match what we did when writing.
      reader.AlignMemory(4);
      const volatile void* plane_pixel_data =
          reader.ExtractReadableMemory(plane_bytes);
      if (!reader.valid())
        return false;
      DCHECK(SkIsAlign4(reinterpret_cast<uintptr_t>(plane_pixel_data)));

      // Const-cast away the "volatile" on |pixel_data|. We specifically
      // understand that a malicious caller may change our pixels under us, and
      // are OK with this as the worst case scenario is visual corruption.
      SkPixmap plane_pixmap(plane_pixmap_info,
                            const_cast<const void*>(plane_pixel_data),
                            plane_stride);
      if (plane_pixmap.computeByteSize() > plane_bytes)
        return false;

      // Nothing should read the colorspace of individual planes because that
      // information is stored in image_, so we pass nullptr.
      sk_sp<SkImage> plane =
          MakeSkImage(plane_pixmap, plane_width, plane_height,
                      nullptr /* target_color_space */);
      if (!plane)
        return false;
      DCHECK(plane->isTextureBacked());

      const size_t plane_size = GrContext::ComputeImageSize(plane, gr_mips);
      size_ += plane_size;
      plane_sizes_.push_back(plane_size);

      // |plane_images_| must be set for use in EnsureMips(), memory dumps, and
      // unit tests.
      plane_images_.push_back(std::move(plane));
    }
    DCHECK(yuv_color_space_.has_value());
    image_ = MakeYUVImageFromUploadedPlanes(
        context_, plane_images_, plane_images_format_, yuv_color_space_.value(),
        decoded_color_space);
    return !!image_;
  }

  SkColorType color_type = kUnknown_SkColorType;
  reader.Read(&color_type);

  if (color_type == kUnknown_SkColorType ||
      color_type == kRGB_101010x_SkColorType ||
      color_type > kLastEnum_SkColorType)
    return false;

  uint32_t width;
  reader.Read(&width);
  uint32_t height;
  reader.Read(&height);
  uint32_t needs_mips;
  reader.Read(&needs_mips);
  has_mips_ = needs_mips;
  size_t pixel_size;
  reader.ReadSize(&pixel_size);
  size_t row_bytes;
  reader.ReadSize(&row_bytes);
  sk_sp<SkColorSpace> pixmap_color_space;
  reader.Read(&pixmap_color_space);
  sk_sp<SkColorSpace> target_color_space;
  reader.Read(&target_color_space);

  if (!reader.valid())
    return false;

  SkImageInfo image_info = SkImageInfo::Make(
      width, height, color_type, kPremul_SkAlphaType, pixmap_color_space);
  if (row_bytes < image_info.minRowBytes() ||
      image_info.computeByteSize(row_bytes) > pixel_size) {
    return false;
  }

  // Align data to a 4-byte boundry, to match what we did when writing.
  reader.AlignMemory(4);
  const volatile void* pixel_data = reader.ExtractReadableMemory(pixel_size);
  if (!reader.valid())
    return false;

  DCHECK(SkIsAlign4(reinterpret_cast<uintptr_t>(pixel_data)));

  if (width == 0 || height == 0)
    return false;

  // Const-cast away the "volatile" on |pixel_data|. We specifically understand
  // that a malicious caller may change our pixels under us, and are OK with
  // this as the worst case scenario is visual corruption.
  SkPixmap pixmap(image_info, const_cast<const void*>(pixel_data), row_bytes);
  image_ = MakeSkImage(pixmap, width, height, target_color_space);

  if (image_) {
    // Match GrTexture::onGpuMemorySize so that memory traces agree.
    auto gr_mips = has_mips_ ? GrMipMapped::kYes : GrMipMapped::kNo;
    size_ = GrContext::ComputeImageSize(image_, gr_mips);
  }

  return !!image_;
}

sk_sp<SkImage> ServiceImageTransferCacheEntry::MakeSkImage(
    const SkPixmap& pixmap,
    uint32_t width,
    uint32_t height,
    sk_sp<SkColorSpace> target_color_space) {
  DCHECK(context_);

  // Depending on whether the pixmap will fit in a GPU texture, either create
  // a software or GPU SkImage.
  uint32_t max_size = context_->maxTextureSize();
  fits_on_gpu_ = width <= max_size && height <= max_size;
  sk_sp<SkImage> image;
  if (fits_on_gpu_) {
    image = SkImage::MakeFromRaster(pixmap, nullptr, nullptr);
    if (!image)
      return nullptr;
    image = MakeTextureImage(context_, std::move(image), target_color_space,
                             has_mips_ ? GrMipMapped::kYes : GrMipMapped::kNo);
  } else {
    // If the image is on the CPU, no work is needed to generate mips.
    has_mips_ = true;
    sk_sp<SkImage> original =
        SkImage::MakeFromRaster(pixmap, [](const void*, void*) {}, nullptr);
    if (!original)
      return nullptr;
    if (target_color_space) {
      image = original->makeColorSpace(target_color_space);
      // If color space conversion is a noop, use original data.
      if (image == original)
        image = SkImage::MakeRasterCopy(pixmap);
    } else {
      // No color conversion to do, use original data.
      image = SkImage::MakeRasterCopy(pixmap);
    }
  }

  // Make sure the GPU work to create the backing texture is issued.
  if (image)
    image->getBackendTexture(true /* flushPendingGrContextIO */);

  return image;
}

const sk_sp<SkImage>& ServiceImageTransferCacheEntry::GetPlaneImage(
    size_t index) const {
  DCHECK_GE(index, 0u);
  DCHECK_LT(index, plane_images_.size());
  DCHECK(plane_images_.at(index));
  return plane_images_.at(index);
}

void ServiceImageTransferCacheEntry::EnsureMips() {
  if (has_mips_)
    return;

  DCHECK(fits_on_gpu_);
  if (is_yuv()) {
    DCHECK(image_);
    DCHECK(yuv_color_space_.has_value());
    DCHECK_NE(YUVDecodeFormat::kUnknown, plane_images_format_);
    DCHECK_EQ(NumberOfPlanesForYUVDecodeFormat(plane_images_format_),
              plane_images_.size());

    // We first do all the work with local variables. Then, if everything
    // succeeds, we update the object's state. That way, we don't leave it in an
    // inconsistent state if one step of mip generation fails.
    std::vector<sk_sp<SkImage>> mipped_planes;
    std::vector<size_t> mipped_plane_sizes;
    for (size_t plane = 0; plane < plane_images_.size(); plane++) {
      DCHECK(plane_images_.at(plane));
      sk_sp<SkImage> mipped_plane = plane_images_.at(plane)->makeTextureImage(
          context_, GrMipMapped::kYes, SkBudgeted::kNo);
      if (!mipped_plane)
        return;
      mipped_planes.push_back(std::move(mipped_plane));
      mipped_plane_sizes.push_back(
          GrContext::ComputeImageSize(mipped_planes.back(), GrMipMapped::kYes));
    }
    sk_sp<SkImage> mipped_image = MakeYUVImageFromUploadedPlanes(
        context_, mipped_planes, plane_images_format_, yuv_color_space_.value(),
        image_->refColorSpace() /* image_color_space */);
    if (!mipped_image)
      return;
    // Note that we cannot update |size_| because the transfer cache keeps track
    // of a total size that is not updated after EnsureMips(). The original size
    // is used when the image is deleted from the cache.
    plane_images_ = std::move(mipped_planes);
    plane_sizes_ = std::move(mipped_plane_sizes);
    image_ = std::move(mipped_image);
    has_mips_ = true;
    return;
  }

  sk_sp<SkImage> mipped_image =
      image_->makeTextureImage(context_, GrMipMapped::kYes, SkBudgeted::kNo);
  if (!mipped_image)
    return;
  image_ = std::move(mipped_image);
  has_mips_ = true;
}

}  // namespace cc
