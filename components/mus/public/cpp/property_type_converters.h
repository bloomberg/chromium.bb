// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_PROPERTY_TYPE_CONVERTERS_H_
#define COMPONENTS_MUS_PUBLIC_CPP_PROPERTY_TYPE_CONVERTERS_H_

#include <stdint.h>
#include <vector>

#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/type_converter.h"

class SkBitmap;

namespace gfx {
class Rect;
class Size;
}

namespace mojo {

// TODO(beng): these methods serialize types used for standard properties
//             to vectors of bytes used by Window::SetSharedProperty().
//             replace this with serialization code generated @ bindings.
//             This would be especially useful for SkBitmap, which could be
//             replaced with the skia.Bitmap mojom struct serialization.

template <>
struct TypeConverter<const std::vector<uint8_t>, gfx::Rect> {
  static const std::vector<uint8_t> Convert(const gfx::Rect& input);
};
template <>
struct TypeConverter<gfx::Rect, const std::vector<uint8_t>> {
  static gfx::Rect Convert(const std::vector<uint8_t>& input);
};

template <>
struct TypeConverter<const std::vector<uint8_t>, gfx::Size> {
  static const std::vector<uint8_t> Convert(const gfx::Size& input);
};
template <>
struct TypeConverter<gfx::Size, const std::vector<uint8_t>> {
  static gfx::Size Convert(const std::vector<uint8_t>& input);
};

template <>
struct TypeConverter<const std::vector<uint8_t>, int32_t> {
  static const std::vector<uint8_t> Convert(const int32_t& input);
};
template <>
struct TypeConverter<int32_t, const std::vector<uint8_t>> {
  static int32_t Convert(const std::vector<uint8_t>& input);
};

template <>
struct TypeConverter<const std::vector<uint8_t>, base::string16> {
  static const std::vector<uint8_t> Convert(const base::string16& input);
};
template <>
struct TypeConverter<base::string16, const std::vector<uint8_t>> {
  static base::string16 Convert(const std::vector<uint8_t>& input);
};

template <>
struct TypeConverter<const std::vector<uint8_t>, std::string> {
  static const std::vector<uint8_t> Convert(const std::string& input);
};
template <>
struct TypeConverter<std::string, const std::vector<uint8_t>> {
  static std::string Convert(const std::vector<uint8_t>& input);
};

// NOTE: These methods only serialize and deserialize the common case of RGBA
// 8888 bitmaps with premultiplied alpha.
template <>
struct TypeConverter<const std::vector<uint8_t>, SkBitmap> {
  static const std::vector<uint8_t> Convert(const SkBitmap& input);
};
template <>
struct TypeConverter<SkBitmap, const std::vector<uint8_t>> {
  static SkBitmap Convert(const std::vector<uint8_t>& input);
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_PUBLIC_CPP_PROPERTY_TYPE_CONVERTERS_H_
