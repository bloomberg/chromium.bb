// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_writer.h"

#include "cc/paint/draw_image.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/paint_typeface_transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {
void TypefaceCataloger(SkTypeface* typeface, void* ctx) {
  static_cast<TransferCacheSerializeHelper*>(ctx)->AssertLocked(
      TransferCacheEntryType::kPaintTypeface, typeface->uniqueID());
}
}  // namespace

PaintOpWriter::PaintOpWriter(void* memory,
                             size_t size,
                             TransferCacheSerializeHelper* transfer_cache,
                             ImageProvider* image_provider,
                             bool enable_security_constraints)
    : memory_(static_cast<char*>(memory) + HeaderBytes()),
      size_(size),
      remaining_bytes_(size - HeaderBytes()),
      transfer_cache_(transfer_cache),
      image_provider_(image_provider),
      enable_security_constraints_(enable_security_constraints) {
  // Leave space for header of type/skip.
  DCHECK_GE(size, HeaderBytes());
}
PaintOpWriter::~PaintOpWriter() = default;

template <typename T>
void PaintOpWriter::WriteSimple(const T& val) {
  static_assert(base::is_trivially_copyable<T>::value, "");
  if (remaining_bytes_ < sizeof(T))
    valid_ = false;
  if (!valid_)
    return;

  reinterpret_cast<T*>(memory_)[0] = val;

  memory_ += sizeof(T);
  remaining_bytes_ -= sizeof(T);
}

void PaintOpWriter::WriteFlattenable(const SkFlattenable* val) {
  DCHECK(SkIsAlign4(reinterpret_cast<uintptr_t>(memory_)))
      << "Flattenable must start writing at 4 byte alignment.";

  if (!val) {
    WriteSize(static_cast<size_t>(0u));
    return;
  }

  sk_sp<SkData> data = val->serialize();
  WriteSize(data->size());
  if (!data->isEmpty())
    WriteData(data->size(), data->data());
}

void PaintOpWriter::WriteSize(size_t size) {
  WriteSimple(size);
}

void PaintOpWriter::Write(SkScalar data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(uint8_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(uint32_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(uint64_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(int32_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(const SkRect& rect) {
  WriteSimple(rect);
}

void PaintOpWriter::Write(const SkIRect& rect) {
  WriteSimple(rect);
}

void PaintOpWriter::Write(const SkRRect& rect) {
  WriteSimple(rect);
}

void PaintOpWriter::Write(const SkPath& path) {
  AlignMemory(4);
  size_t bytes = path.writeToMemory(nullptr);
  if (bytes > remaining_bytes_)
    valid_ = false;
  if (!valid_)
    return;

  size_t bytes_written = path.writeToMemory(memory_);
  DCHECK_LE(bytes_written, bytes);
  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpWriter::Write(const PaintFlags& flags) {
  Write(flags.text_size_);
  WriteSimple(flags.color_);
  Write(flags.width_);
  Write(flags.miter_limit_);
  WriteSimple(flags.blend_mode_);
  WriteSimple(flags.bitfields_uint_);

  // TODO(enne): WriteTypeface, http://crbug.com/737629

  // Flattenables must be written starting at a 4 byte boundary, which should be
  // the case here.
  WriteFlattenable(flags.path_effect_.get());
  AlignMemory(4);
  WriteFlattenable(flags.mask_filter_.get());
  AlignMemory(4);
  WriteFlattenable(flags.color_filter_.get());

  AlignMemory(4);
  if (enable_security_constraints_)
    WriteSize(static_cast<size_t>(0u));
  else
    WriteFlattenable(flags.draw_looper_.get());

  Write(flags.image_filter_.get());
  Write(flags.shader_.get());
}

void PaintOpWriter::Write(const DrawImage& image) {
  if (enable_security_constraints_) {
    SkBitmap bm;
    if (!image.paint_image().GetSkImage()->asLegacyBitmap(
            &bm, SkImage::LegacyBitmapMode::kRO_LegacyBitmapMode)) {
      Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kNoImage));
      return;
    }

    Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kImageData));
    const auto& pixmap = bm.pixmap();
    Write(pixmap.colorType());
    Write(pixmap.width());
    Write(pixmap.height());
    size_t pixmap_size = pixmap.computeByteSize();
    WriteSize(pixmap_size);
    WriteData(pixmap_size, pixmap.addr());
    return;
  }

  Write(
      static_cast<uint8_t>(PaintOp::SerializedImageType::kTransferCacheEntry));
  auto decoded_image = image_provider_->GetDecodedDrawImage(image);
  base::Optional<uint32_t> id =
      decoded_image.decoded_image().transfer_cache_entry_id();

  // In the case of a decode failure, id may not be set. Send an invalid ID.
  Write(id ? *id : kInvalidImageTransferCacheEntryId);
}

void PaintOpWriter::Write(const sk_sp<SkData>& data) {
  if (data.get() && data->size()) {
    WriteSize(data->size());
    WriteData(data->size(), data->data());
  } else {
    // Differentiate between nullptr and valid but zero size.  It's not clear
    // that this happens in practice, but seems better to be consistent.
    WriteSize(static_cast<size_t>(0));
    Write(!!data.get());
  }
}

void PaintOpWriter::Write(const sk_sp<SkTextBlob>& blob) {
  auto data = blob->serialize(&TypefaceCataloger, transfer_cache_);
  Write(data);
}

void PaintOpWriter::Write(const scoped_refptr<PaintTextBlob>& blob) {
  if (!valid_)
    return;

  for (auto& typeface : blob->typefaces()) {
    auto locked = transfer_cache_->LockEntry(
        TransferCacheEntryType::kPaintTypeface, typeface.sk_id());
    if (locked)
      continue;
    transfer_cache_->CreateEntry(
        ClientPaintTypefaceTransferCacheEntry(typeface));
    transfer_cache_->AssertLocked(TransferCacheEntryType::kPaintTypeface,
                                  typeface.sk_id());
  }

  Write(blob->ToSkTextBlob());
}

void PaintOpWriter::Write(const PaintShader* shader) {
  if (!shader) {
    WriteSimple(false);
    return;
  }

  // TODO(vmpstr): This could be optimized to only serialize fields relevant to
  // the specific shader type. If done, then corresponding reading and tests
  // would have to also be updated.
  WriteSimple(true);
  WriteSimple(shader->shader_type_);
  WriteSimple(shader->flags_);
  WriteSimple(shader->end_radius_);
  WriteSimple(shader->start_radius_);
  WriteSimple(shader->tx_);
  WriteSimple(shader->ty_);
  WriteSimple(shader->fallback_color_);
  WriteSimple(shader->scaling_behavior_);
  if (shader->local_matrix_) {
    Write(true);
    WriteSimple(*shader->local_matrix_);
  } else {
    Write(false);
  }
  WriteSimple(shader->center_);
  WriteSimple(shader->tile_);
  WriteSimple(shader->start_point_);
  WriteSimple(shader->end_point_);
  WriteSimple(shader->start_degrees_);
  WriteSimple(shader->end_degrees_);
  Write(shader->image_);
  if (shader->record_) {
    Write(true);
    base::Optional<gfx::Rect> playback_rect;
    base::Optional<gfx::SizeF> post_scale;
    if (shader->tile_scale()) {
      playback_rect.emplace(
          gfx::ToEnclosingRect(gfx::SkRectToRectF(shader->tile())));
      post_scale.emplace(*shader->tile_scale());
    }
    Write(shader->record_.get(), std::move(playback_rect),
          std::move(post_scale));
  } else {
    Write(false);
  }
  WriteSimple(shader->colors_.size());
  WriteData(shader->colors_.size() * sizeof(SkColor), shader->colors_.data());

  WriteSimple(shader->positions_.size());
  WriteData(shader->positions_.size() * sizeof(SkScalar),
            shader->positions_.data());
  // Explicitly don't write the cached_shader_ because that can be regenerated
  // using other fields.
}

void PaintOpWriter::Write(SkColorType color_type) {
  WriteSimple(static_cast<uint32_t>(color_type));
}

void PaintOpWriter::WriteData(size_t bytes, const void* input) {
  if (bytes > remaining_bytes_)
    valid_ = false;
  if (!valid_)
    return;
  if (bytes == 0)
    return;

  memcpy(memory_, input, bytes);
  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpWriter::WriteArray(size_t count, const SkPoint* input) {
  size_t bytes = sizeof(SkPoint) * count;
  WriteData(bytes, input);
}

void PaintOpWriter::AlignMemory(size_t alignment) {
  // Due to the math below, alignment must be a power of two.
  DCHECK_GT(alignment, 0u);
  DCHECK_EQ(alignment & (alignment - 1), 0u);

  uintptr_t memory = reinterpret_cast<uintptr_t>(memory_);
  // The following is equivalent to:
  //   padding = (alignment - memory % alignment) % alignment;
  // because alignment is a power of two. This doesn't use modulo operator
  // however, since it can be slow.
  size_t padding = ((memory + alignment - 1) & ~(alignment - 1)) - memory;
  if (padding > remaining_bytes_)
    valid_ = false;

  memory_ += padding;
  remaining_bytes_ -= padding;
}

void PaintOpWriter::Write(const PaintFilter* filter) {
  if (!filter) {
    WriteSimple(static_cast<uint32_t>(PaintFilter::Type::kNullFilter));
    return;
  }
  WriteSimple(static_cast<uint32_t>(filter->type()));
  auto* crop_rect = filter->crop_rect();
  WriteSimple(static_cast<uint32_t>(!!crop_rect));
  if (crop_rect) {
    WriteSimple(crop_rect->flags());
    WriteSimple(crop_rect->rect());
  }

  if (!valid_)
    return;

  AlignMemory(4);
  switch (filter->type()) {
    case PaintFilter::Type::kNullFilter:
      NOTREACHED();
      break;
    case PaintFilter::Type::kColorFilter:
      Write(static_cast<const ColorFilterPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kBlur:
      Write(static_cast<const BlurPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kDropShadow:
      Write(static_cast<const DropShadowPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMagnifier:
      Write(static_cast<const MagnifierPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kCompose:
      Write(static_cast<const ComposePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kAlphaThreshold:
      Write(static_cast<const AlphaThresholdPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kXfermode:
      Write(static_cast<const XfermodePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kArithmetic:
      Write(static_cast<const ArithmeticPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMatrixConvolution:
      Write(static_cast<const MatrixConvolutionPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kDisplacementMapEffect:
      Write(static_cast<const DisplacementMapEffectPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kImage:
      Write(static_cast<const ImagePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kPaintRecord:
      Write(static_cast<const RecordPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMerge:
      Write(static_cast<const MergePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMorphology:
      Write(static_cast<const MorphologyPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kOffset:
      Write(static_cast<const OffsetPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kTile:
      Write(static_cast<const TilePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kTurbulence:
      Write(static_cast<const TurbulencePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kPaintFlags:
      Write(static_cast<const PaintFlagsPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMatrix:
      Write(static_cast<const MatrixPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kLightingDistant:
      Write(static_cast<const LightingDistantPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kLightingPoint:
      Write(static_cast<const LightingPointPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kLightingSpot:
      Write(static_cast<const LightingSpotPaintFilter&>(*filter));
      break;
  }
}

void PaintOpWriter::Write(const ColorFilterPaintFilter& filter) {
  WriteFlattenable(filter.color_filter().get());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const BlurPaintFilter& filter) {
  WriteSimple(filter.sigma_x());
  WriteSimple(filter.sigma_y());
  WriteSimple(filter.tile_mode());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const DropShadowPaintFilter& filter) {
  WriteSimple(filter.dx());
  WriteSimple(filter.dy());
  WriteSimple(filter.sigma_x());
  WriteSimple(filter.sigma_y());
  WriteSimple(filter.color());
  WriteSimple(filter.shadow_mode());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const MagnifierPaintFilter& filter) {
  WriteSimple(filter.src_rect());
  WriteSimple(filter.inset());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const ComposePaintFilter& filter) {
  Write(filter.outer().get());
  Write(filter.inner().get());
}

void PaintOpWriter::Write(const AlphaThresholdPaintFilter& filter) {
  Write(filter.region());
  WriteSimple(filter.inner_min());
  WriteSimple(filter.outer_max());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const XfermodePaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.blend_mode()));
  Write(filter.background().get());
  Write(filter.foreground().get());
}

void PaintOpWriter::Write(const ArithmeticPaintFilter& filter) {
  WriteSimple(filter.k1());
  WriteSimple(filter.k2());
  WriteSimple(filter.k3());
  WriteSimple(filter.k4());
  WriteSimple(filter.enforce_pm_color());
  Write(filter.background().get());
  Write(filter.foreground().get());
}

void PaintOpWriter::Write(const MatrixConvolutionPaintFilter& filter) {
  WriteSimple(filter.kernel_size());
  auto size = static_cast<size_t>(
      sk_64_mul(filter.kernel_size().width(), filter.kernel_size().height()));
  for (size_t i = 0; i < size; ++i)
    WriteSimple(filter.kernel_at(i));
  WriteSimple(filter.gain());
  WriteSimple(filter.bias());
  WriteSimple(filter.kernel_offset());
  WriteSimple(static_cast<uint32_t>(filter.tile_mode()));
  WriteSimple(filter.convolve_alpha());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const DisplacementMapEffectPaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.channel_x()));
  WriteSimple(static_cast<uint32_t>(filter.channel_y()));
  WriteSimple(filter.scale());
  Write(filter.displacement().get());
  Write(filter.color().get());
}

void PaintOpWriter::Write(const ImagePaintFilter& filter) {
  DrawImage draw_image(
      filter.image(),
      SkIRect::MakeWH(filter.image().width(), filter.image().height()),
      filter.filter_quality(), SkMatrix::I());
  Write(draw_image);
  Write(filter.src_rect());
  Write(filter.dst_rect());
  Write(filter.filter_quality());
}

void PaintOpWriter::Write(const RecordPaintFilter& filter) {
  WriteSimple(filter.record_bounds());
  Write(filter.record().get());
}

void PaintOpWriter::Write(const MergePaintFilter& filter) {
  WriteSimple(filter.input_count());
  for (size_t i = 0; i < filter.input_count(); ++i)
    Write(filter.input_at(i));
}

void PaintOpWriter::Write(const MorphologyPaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.morph_type()));
  WriteSimple(filter.radius_x());
  WriteSimple(filter.radius_y());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const OffsetPaintFilter& filter) {
  WriteSimple(filter.dx());
  WriteSimple(filter.dy());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const TilePaintFilter& filter) {
  WriteSimple(filter.src());
  WriteSimple(filter.dst());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const TurbulencePaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.turbulence_type()));
  WriteSimple(filter.base_frequency_x());
  WriteSimple(filter.base_frequency_y());
  WriteSimple(filter.num_octaves());
  WriteSimple(filter.seed());
  WriteSimple(filter.tile_size());
}

void PaintOpWriter::Write(const PaintFlagsPaintFilter& filter) {
  Write(filter.flags());
}

void PaintOpWriter::Write(const MatrixPaintFilter& filter) {
  WriteSimple(filter.matrix());
  WriteSimple(filter.filter_quality());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const LightingDistantPaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.lighting_type()));
  WriteSimple(filter.direction());
  WriteSimple(filter.light_color());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const LightingPointPaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.lighting_type()));
  WriteSimple(filter.location());
  WriteSimple(filter.light_color());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const LightingSpotPaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.lighting_type()));
  WriteSimple(filter.location());
  WriteSimple(filter.target());
  WriteSimple(filter.specular_exponent());
  WriteSimple(filter.cutoff_angle());
  WriteSimple(filter.light_color());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const PaintRecord* record,
                          base::Optional<gfx::Rect> playback_rect,
                          base::Optional<gfx::SizeF> post_scale) {
  DCHECK_EQ(playback_rect.has_value(), post_scale.has_value());
  // We need to record how many bytes we will serialize, but we don't know this
  // information until we do the serialization. So, skip the amount needed
  // before writing.
  size_t size_offset = sizeof(size_t);
  if (size_offset > remaining_bytes_) {
    valid_ = false;
    return;
  }

  char* size_memory = memory_;

  memory_ += size_offset;
  remaining_bytes_ -= size_offset;
  AlignMemory(PaintOpBuffer::PaintOpAlign);
  if (!valid_)
    return;

  if (enable_security_constraints_) {
    // We don't serialize PaintRecords when security constraints are enabled.
    reinterpret_cast<size_t*>(size_memory)[0] = 0u;
    return;
  }

  SimpleBufferSerializer serializer(memory_, remaining_bytes_, image_provider_,
                                    transfer_cache_);
  if (playback_rect) {
    serializer.Serialize(record, *playback_rect, *post_scale);
  } else {
    serializer.Serialize(record);
  }

  if (!serializer.valid()) {
    valid_ = false;
    return;
  }
  // Now we can write the number of bytes we used. Ensure this amount is size_t,
  // since that's what we allocated for it.
  static_assert(sizeof(serializer.written()) == sizeof(size_t),
                "written() return type size is different from sizeof(size_t)");

  // Write the size to the size memory, which preceeds the memory for the
  // record.
  reinterpret_cast<size_t*>(size_memory)[0] = serializer.written();

  // The serializer should have failed if it ran out of space. DCHECK to verify
  // that it wrote at most as many bytes as we had left.
  DCHECK_LE(serializer.written(), remaining_bytes_);
  memory_ += serializer.written();
  remaining_bytes_ -= serializer.written();
}

void PaintOpWriter::Write(const PaintImage& image) {
  if (!image) {
    Write(static_cast<uint8_t>(
        PaintOp::SerializedImageType::kTransferCacheEntry));
    Write(kInvalidImageTransferCacheEntryId);
    return;
  }
  // Note that the filter quality doesn't matter here, since it's not
  // serialized. It's only used to determine the decoded draw image that we will
  // get. However, since we're requesting a full-sized image decode, the filter
  // quality is essentially ignored.
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kLow_SkFilterQuality, SkMatrix::I());
  Write(draw_image);
}

void PaintOpWriter::Write(const SkRegion& region) {
  size_t bytes_required = region.writeToMemory(nullptr);
  std::unique_ptr<char[]> data(new char[bytes_required]);
  size_t bytes_written = region.writeToMemory(data.get());
  DCHECK_EQ(bytes_required, bytes_written);

  WriteSimple(bytes_written);
  WriteData(bytes_written, data.get());
}

}  // namespace cc
