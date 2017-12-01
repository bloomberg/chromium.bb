// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_writer.h"

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace cc {

PaintOpWriter::PaintOpWriter(void* memory, size_t size)
    : memory_(static_cast<char*>(memory) + HeaderBytes()),
      size_(size),
      remaining_bytes_(size - HeaderBytes()) {
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

  // TODO(enne): change skia API to make this a const parameter.
  sk_sp<SkData> data(
      SkValidatingSerializeFlattenable(const_cast<SkFlattenable*>(val)));

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
  size_t bytes = path.writeToMemory(nullptr);
  if (bytes > remaining_bytes_)
    valid_ = false;
  if (!valid_)
    return;

  path.writeToMemory(memory_);
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
  WriteFlattenable(flags.draw_looper_.get());

  Write(flags.shader_.get());
}

void PaintOpWriter::Write(const PaintImage& image,
                          ImageProvider* image_provider) {
  // TODO(enne): implement PaintImage serialization: http://crbug.com/737629
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

void PaintOpWriter::Write(const std::vector<PaintTypeface>& typefaces) {
  WriteSimple(static_cast<uint32_t>(typefaces.size()));
  for (const auto& typeface : typefaces) {
    DCHECK(typeface);
    WriteSimple(typeface.sk_id());
    WriteSimple(static_cast<uint8_t>(typeface.type()));
    switch (typeface.type()) {
      case PaintTypeface::Type::kTestTypeface:
        // Nothing to serialize here.
        break;
      case PaintTypeface::Type::kSkTypeface:
        // Nothing to do here. This should never be the case when everything is
        // implemented. This should be a NOTREACHED() eventually.
        break;
      case PaintTypeface::Type::kFontConfigInterfaceIdAndTtcIndex:
        WriteSimple(typeface.font_config_interface_id());
        WriteSimple(typeface.ttc_index());
        break;
      case PaintTypeface::Type::kFilenameAndTtcIndex:
        WriteSimple(typeface.filename().size());
        WriteData(typeface.filename().size(), typeface.filename().data());
        WriteSimple(typeface.ttc_index());
        break;
      case PaintTypeface::Type::kFamilyNameAndFontStyle:
        WriteSimple(typeface.family_name().size());
        WriteData(typeface.family_name().size(), typeface.family_name().data());
        WriteSimple(typeface.font_style().weight());
        WriteSimple(typeface.font_style().width());
        WriteSimple(typeface.font_style().slant());
        break;
    }
#if DCHECK_IS_ON()
    if (typeface)
      last_serialized_typeface_ids_.insert(typeface.sk_id());
#endif
  }
}

// static
void PaintOpWriter::TypefaceCataloger(SkTypeface* typeface, void* ctx) {
  DCHECK(static_cast<PaintOpWriter*>(ctx)->last_serialized_typeface_ids_.count(
             typeface->uniqueID()) != 0);
}

void PaintOpWriter::Write(const sk_sp<SkTextBlob>& blob) {
  auto data = blob->serialize(&PaintOpWriter::TypefaceCataloger, this);
  Write(data);

#if DCHECK_IS_ON()
  last_serialized_typeface_ids_.clear();
#endif
}

void PaintOpWriter::Write(const scoped_refptr<PaintTextBlob>& blob) {
  Write(blob->typefaces());
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
  // TODO(vmpstr): Write PaintImage image_. http://crbug.com/737629
  // TODO(vmpstr): Write sk_sp<PaintRecord> record_. http://crbug.com/737629
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

}  // namespace cc
