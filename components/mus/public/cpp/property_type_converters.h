// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_PROPERTY_TYPE_CONVERTERS_H_
#define COMPONENTS_MUS_PUBLIC_CPP_PROPERTY_TYPE_CONVERTERS_H_

#include <stdint.h>
#include <vector>

#include "mojo/public/cpp/bindings/type_converter.h"

namespace gfx {
class Rect;
class Size;
}

namespace mojo {

// TODO(beng): these methods serialize types used for standard properties
//             to vectors of bytes used by Window::SetSharedProperty().
//             replace this with serialization code generated @ bindings.

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

}  // namespace mojo

#endif  // COMPONENTS_MUS_PUBLIC_CPP_PROPERTY_TYPE_CONVERTERS_H_
