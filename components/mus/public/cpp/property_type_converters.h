// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_PROPERTY_TYPE_CONVERTERS_H_
#define COMPONENTS_MUS_PUBLIC_CPP_PROPERTY_TYPE_CONVERTERS_H_

#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "ui/mojo/geometry/geometry.mojom.h"

namespace mojo {

// TODO(beng): these methods serialize types used for standard properties
//             to vectors of bytes used by Window::SetSharedProperty().
//             replace this with serialization code generated @ bindings.

template <>
struct TypeConverter<const std::vector<uint8_t>, Rect> {
  static const std::vector<uint8_t> Convert(const Rect& input);
};
template <>
struct TypeConverter<Rect, const std::vector<uint8_t>> {
  static Rect Convert(const std::vector<uint8_t>& input);
};

template <>
struct TypeConverter<const std::vector<uint8_t>, Size> {
  static const std::vector<uint8_t> Convert(const Size& input);
};
template <>
struct TypeConverter<Size, const std::vector<uint8_t>> {
  static Size Convert(const std::vector<uint8_t>& input);
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
