// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_writer.h"

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace cc {

template <typename T>
void PaintOpWriter::WriteSimple(const T& val) {
  static_assert(base::is_trivially_copyable<T>::value, "");
  if (!AlignMemory(alignof(T)))
    valid_ = false;
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
  // TODO(enne): change skia API to make this a const parameter.
  sk_sp<SkData> data(
      SkValidatingSerializeFlattenable(const_cast<SkFlattenable*>(val)));

  Write(data->size());
  if (!data->isEmpty())
    WriteData(data->size(), data->data());
}

void PaintOpWriter::Write(size_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(SkScalar data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(uint8_t data) {
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
  WriteFlattenable(flags.mask_filter_.get());
  WriteFlattenable(flags.color_filter_.get());
  WriteFlattenable(flags.draw_looper_.get());
  WriteFlattenable(flags.image_filter_.get());

  Write(flags.shader_.get());
}

void PaintOpWriter::Write(const PaintImage& image, ImageDecodeCache* cache) {
  // TODO(enne): implement PaintImage serialization: http://crbug.com/737629
}

void PaintOpWriter::Write(const sk_sp<SkData>& data) {
  if (data.get() && data->size()) {
    Write(data->size());
    WriteData(data->size(), data->data());
  } else {
    // Differentiate between nullptr and valid but zero size.  It's not clear
    // that this happens in practice, but seems better to be consistent.
    Write(static_cast<size_t>(0));
    Write(!!data.get());
  }
}

void PaintOpWriter::Write(const sk_sp<SkTextBlob>& blob) {
  // TODO(enne): implement SkTextBlob serialization: http://crbug.com/737629
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

bool PaintOpWriter::AlignMemory(size_t alignment) {
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
    return false;

  memory_ += padding;
  remaining_bytes_ -= padding;
  return true;
}

}  // namespace cc
