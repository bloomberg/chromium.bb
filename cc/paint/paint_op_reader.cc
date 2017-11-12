// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_reader.h"

#include <stddef.h>
#include <algorithm>

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace cc {
namespace {

uint32_t kMaxTypefacesCount = 128;
size_t kMaxFilenameSize = 1024;
size_t kMaxFamilyNameSize = 128;

// If we have more than this many colors, abort deserialization.
const size_t kMaxShaderColorsSupported = 10000;

struct TypefacesCatalog {
  const std::vector<PaintTypeface>* typefaces;
  bool had_null = false;
};

sk_sp<SkTypeface> ResolveTypeface(uint32_t id, void* ctx) {
  TypefacesCatalog* catalog = static_cast<TypefacesCatalog*>(ctx);
  auto typeface_it = std::find_if(
      catalog->typefaces->begin(), catalog->typefaces->end(),
      [id](const PaintTypeface& typeface) { return typeface.sk_id() == id; });
  // TODO(vmpstr): The !*typeface check is here because not all typefaces are
  // supported right now. Instead of making the reader invalid during the
  // typeface deserialization, which results in an invalid op, instead just make
  // the textblob be null by setting |had_null| to true.
  if (typeface_it == catalog->typefaces->end() || !*typeface_it) {
    catalog->had_null = true;
    return nullptr;
  }
  return typeface_it->ToSkTypeface();
}

bool IsValidPaintShaderType(PaintShader::Type type) {
  return static_cast<uint8_t>(type) <
         static_cast<uint8_t>(PaintShader::Type::kShaderCount);
}

bool IsValidSkShaderTileMode(SkShader::TileMode mode) {
  return mode < SkShader::kTileModeCount;
}

bool IsValidPaintShaderScalingBehavior(PaintShader::ScalingBehavior behavior) {
  return behavior == PaintShader::ScalingBehavior::kRasterAtScale ||
         behavior == PaintShader::ScalingBehavior::kFixedScale;
}

}  // namespace

// static
void PaintOpReader::FixupMatrixPostSerialization(SkMatrix* matrix) {
  // Can't trust malicious clients to provide the correct derived matrix type.
  // However, if a matrix thinks that it's identity, then make it so.
  if (matrix->isIdentity())
    matrix->setIdentity();
  else
    matrix->dirtyMatrixTypeCache();
}

// static
bool PaintOpReader::ReadAndValidateOpHeader(const volatile void* input,
                                            size_t input_size,
                                            uint8_t* type,
                                            uint32_t* skip) {
  uint32_t first_word = reinterpret_cast<const volatile uint32_t*>(input)[0];
  *type = static_cast<uint8_t>(first_word & 0xFF);
  *skip = first_word >> 8;

  if (input_size < *skip)
    return false;
  if (*skip % PaintOpBuffer::PaintOpAlign != 0)
    return false;
  if (*type > static_cast<uint8_t>(PaintOpType::LastPaintOpType))
    return false;
  return true;
}

template <typename T>
void PaintOpReader::ReadSimple(T* val) {
  static_assert(base::is_trivially_copyable<T>::value,
                "Not trivially copyable");
  if (!AlignMemory(alignof(T)))
    valid_ = false;
  if (remaining_bytes_ < sizeof(T))
    valid_ = false;
  if (!valid_)
    return;

  // Most of the time this is used for primitives, but this function is also
  // used for SkRect/SkIRect/SkMatrix whose implicit operator= can't use a
  // volatile.  TOCTOU violations don't matter for these simple types so
  // use assignment.
  *val = *reinterpret_cast<const T*>(const_cast<const char*>(memory_));

  memory_ += sizeof(T);
  remaining_bytes_ -= sizeof(T);
}

template <typename T>
void PaintOpReader::ReadFlattenable(sk_sp<T>* val) {
  size_t bytes = 0;
  ReadSimple(&bytes);
  if (remaining_bytes_ < bytes)
    valid_ = false;
  if (!SkIsAlign4(reinterpret_cast<uintptr_t>(memory_)))
    valid_ = false;
  if (!valid_)
    return;
  if (bytes == 0)
    return;

  // This is assumed safe from TOCTOU violations as the flattenable
  // deserializing function uses an SkReadBuffer which reads each piece of
  // memory once much like PaintOpReader does.
  val->reset(static_cast<T*>(SkValidatingDeserializeFlattenable(
      const_cast<const char*>(memory_), bytes, T::GetFlattenableType())));
  if (!val)
    valid_ = false;

  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpReader::ReadData(size_t bytes, void* data) {
  if (remaining_bytes_ < bytes)
    valid_ = false;
  if (!valid_)
    return;
  if (bytes == 0)
    return;

  memcpy(data, const_cast<const char*>(memory_), bytes);
  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpReader::ReadArray(size_t count, SkPoint* array) {
  size_t bytes = count * sizeof(SkPoint);
  if (remaining_bytes_ < bytes)
    valid_ = false;
  // Overflow?
  if (count > static_cast<size_t>(~0) / sizeof(SkPoint))
    valid_ = false;
  if (!valid_)
    return;
  if (count == 0)
    return;

  memcpy(array, const_cast<const char*>(memory_), bytes);
  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpReader::Read(SkScalar* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(size_t* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(uint8_t* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(SkRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkIRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkRRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkPath* path) {
  if (!valid_)
    return;

  // This is assumed safe from TOCTOU violations as the SkPath deserializing
  // function uses an SkRBuffer which reads each piece of memory once much
  // like PaintOpReader does.  Additionally, paths are later validated in
  // PaintOpBuffer.
  size_t read_bytes =
      path->readFromMemory(const_cast<const char*>(memory_), remaining_bytes_);
  if (!read_bytes)
    valid_ = false;

  memory_ += read_bytes;
  remaining_bytes_ -= read_bytes;
}

void PaintOpReader::Read(PaintFlags* flags) {
  Read(&flags->text_size_);
  ReadSimple(&flags->color_);
  Read(&flags->width_);
  Read(&flags->miter_limit_);
  ReadSimple(&flags->blend_mode_);
  ReadSimple(&flags->bitfields_uint_);

  // TODO(enne): ReadTypeface, http://crbug.com/737629

  // Flattenables must be read at 4-byte boundary, which should be the case
  // here.
  ReadFlattenable(&flags->path_effect_);
  ReadFlattenable(&flags->mask_filter_);
  ReadFlattenable(&flags->color_filter_);
  ReadFlattenable(&flags->draw_looper_);
  ReadFlattenable(&flags->image_filter_);

  Read(&flags->shader_);
}

void PaintOpReader::Read(PaintImage* image) {
  // TODO(enne): implement PaintImage serialization: http://crbug.com/737629
}

void PaintOpReader::Read(sk_sp<SkData>* data) {
  size_t bytes = 0;
  ReadSimple(&bytes);
  if (remaining_bytes_ < bytes)
    valid_ = false;
  if (!valid_)
    return;

  // Separate out empty vs not valid cases.
  if (bytes == 0) {
    bool has_data = false;
    Read(&has_data);
    if (has_data)
      *data = SkData::MakeEmpty();
    return;
  }

  // This is safe to cast away the volatile as it is just a memcpy internally.
  *data = SkData::MakeWithCopy(const_cast<const char*>(memory_), bytes);

  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpReader::Read(std::vector<PaintTypeface>* typefaces) {
  uint32_t typefaces_count;
  ReadSimple(&typefaces_count);
  if (!valid_ || typefaces_count > kMaxTypefacesCount) {
    valid_ = false;
    return;
  }
  typefaces->reserve(typefaces_count);
  for (uint32_t i = 0; i < typefaces_count; ++i) {
    SkFontID id;
    uint8_t type;
    PaintTypeface typeface;
    ReadSimple(&id);
    ReadSimple(&type);
    if (!valid_)
      return;
    switch (static_cast<PaintTypeface::Type>(type)) {
      case PaintTypeface::Type::kTestTypeface:
        typeface = PaintTypeface::TestTypeface();
        break;
      case PaintTypeface::Type::kSkTypeface:
        // TODO(vmpstr): This shouldn't ever happen once everything is
        // implemented. So this should be a failure (ie |valid_| = false).
        break;
      case PaintTypeface::Type::kFontConfigInterfaceIdAndTtcIndex: {
        int font_config_interface_id;
        int ttc_index;
        ReadSimple(&font_config_interface_id);
        ReadSimple(&ttc_index);
        typeface = PaintTypeface::FromFontConfigInterfaceIdAndTtcIndex(
            font_config_interface_id, ttc_index);
        break;
      }
      case PaintTypeface::Type::kFilenameAndTtcIndex: {
        size_t size;
        ReadSimple(&size);
        if (!valid_ || size > kMaxFilenameSize) {
          valid_ = false;
          return;
        }

        std::unique_ptr<char[]> buffer(new char[size]);
        ReadData(size, buffer.get());
        std::string filename(buffer.get(), size);

        int ttc_index;
        ReadSimple(&ttc_index);
        typeface = PaintTypeface::FromFilenameAndTtcIndex(filename, ttc_index);
        break;
      }
      case PaintTypeface::Type::kFamilyNameAndFontStyle: {
        size_t size;
        ReadSimple(&size);
        if (!valid_ || size > kMaxFamilyNameSize) {
          valid_ = false;
          return;
        }

        std::unique_ptr<char[]> buffer(new char[size]);
        ReadData(size, buffer.get());
        std::string family_name(buffer.get(), size);

        int weight;
        int width;
        SkFontStyle::Slant slant;
        ReadSimple(&weight);
        ReadSimple(&width);
        ReadSimple(&slant);
        typeface = PaintTypeface::FromFamilyNameAndFontStyle(
            family_name, SkFontStyle(weight, width, slant));
        break;
      }
    }
    typeface.SetSkId(id);
    typefaces->emplace_back(std::move(typeface));
  }
}

void PaintOpReader::Read(const std::vector<PaintTypeface>& typefaces,
                         sk_sp<SkTextBlob>* blob) {
  sk_sp<SkData> data;
  Read(&data);
  if (!data || !valid_)
    return;

  // Skia expects the following to be true, make sure we don't pass it incorrect
  // data.
  if (!data->data() || !SkIsAlign4(data->size())) {
    valid_ = false;
    return;
  }

  TypefacesCatalog catalog;
  catalog.typefaces = &typefaces;
  *blob = SkTextBlob::Deserialize(data->data(), data->size(), &ResolveTypeface,
                                  &catalog);
  if (catalog.had_null)
    *blob = nullptr;
}

void PaintOpReader::Read(scoped_refptr<PaintTextBlob>* paint_blob) {
  std::vector<PaintTypeface> typefaces;
  sk_sp<SkTextBlob> blob;

  Read(&typefaces);
  Read(typefaces, &blob);
  // TODO(vmpstr): If we couldn't serialize |blob|, we should make |paint_blob|
  // nullptr. However, this causes GL errors right now, because not all
  // typefaces are serialized. Fix this once we serialize everything. For now
  // the behavior is that the |paint_blob| op exists and is valid, but
  // internally it has a nullptr SkTextBlob which skia ignores.
  *paint_blob = base::MakeRefCounted<PaintTextBlob>(std::move(blob),
                                                    std::move(typefaces));
}

void PaintOpReader::Read(sk_sp<PaintShader>* shader) {
  bool has_shader = false;
  ReadSimple(&has_shader);
  if (!has_shader) {
    *shader = nullptr;
    return;
  }
  PaintShader::Type shader_type;
  ReadSimple(&shader_type);
  // Avoid creating a shader if something is invalid.
  if (!valid_ || !IsValidPaintShaderType(shader_type)) {
    valid_ = false;
    return;
  }

  *shader = sk_sp<PaintShader>(new PaintShader(shader_type));
  PaintShader& ref = **shader;
  ReadSimple(&ref.flags_);
  ReadSimple(&ref.end_radius_);
  ReadSimple(&ref.start_radius_);
  ReadSimple(&ref.tx_);
  ReadSimple(&ref.ty_);
  if (!IsValidSkShaderTileMode(ref.tx_) || !IsValidSkShaderTileMode(ref.ty_))
    valid_ = false;
  ReadSimple(&ref.fallback_color_);
  ReadSimple(&ref.scaling_behavior_);
  if (!IsValidPaintShaderScalingBehavior(ref.scaling_behavior_))
    valid_ = false;
  bool has_local_matrix = false;
  ReadSimple(&has_local_matrix);
  if (has_local_matrix) {
    ref.local_matrix_.emplace();
    Read(&*ref.local_matrix_);
  }
  ReadSimple(&ref.center_);
  ReadSimple(&ref.tile_);
  ReadSimple(&ref.start_point_);
  ReadSimple(&ref.end_point_);
  ReadSimple(&ref.start_degrees_);
  ReadSimple(&ref.end_degrees_);
  // TODO(vmpstr): Read PaintImage image_. http://crbug.com/737629
  // TODO(vmpstr): Read sk_sp<PaintRecord> record_. http://crbug.com/737629
  decltype(ref.colors_)::size_type colors_size = 0;
  ReadSimple(&colors_size);

  // If there are too many colors, abort.
  if (colors_size > kMaxShaderColorsSupported) {
    valid_ = false;
    return;
  }
  size_t colors_bytes = colors_size * sizeof(SkColor);
  if (colors_bytes > remaining_bytes_) {
    valid_ = false;
    return;
  }
  ref.colors_.resize(colors_size);
  ReadData(colors_bytes, ref.colors_.data());

  decltype(ref.positions_)::size_type positions_size = 0;
  ReadSimple(&positions_size);
  // Positions are optional. If they exist, they have the same count as colors.
  if (positions_size > 0 && positions_size != colors_size) {
    valid_ = false;
    return;
  }
  size_t positions_bytes = positions_size * sizeof(SkScalar);
  if (positions_bytes > remaining_bytes_) {
    valid_ = false;
    return;
  }
  ref.positions_.resize(positions_size);
  ReadData(positions_size * sizeof(SkScalar), ref.positions_.data());

  // We don't write the cached shader, so don't attempt to read it either.

  // TODO(vmpstr): add serialization of these shader types.  For the moment
  // pretend that these shaders don't exist and that the serialization is
  // successful.  Valid ops pre-serialization should not cause deserialization
  // failures.
  if (shader_type == PaintShader::Type::kPaintRecord ||
      shader_type == PaintShader::Type::kImage) {
    *shader = nullptr;
    return;
  }

  if (!(*shader)->IsValid()) {
    valid_ = false;
  }
}

void PaintOpReader::Read(SkMatrix* matrix) {
  ReadSimple(matrix);
  FixupMatrixPostSerialization(matrix);
}

bool PaintOpReader::AlignMemory(size_t alignment) {
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
