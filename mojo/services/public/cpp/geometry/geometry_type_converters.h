// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_

#include "mojo/services/public/cpp/geometry/mojo_geometry_export.h"
#include "mojo/services/public/interfaces/geometry/geometry.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {

template<>
class MOJO_GEOMETRY_EXPORT TypeConverter<Point, gfx::Point> {
 public:
  static Point ConvertFrom(const gfx::Point& input, Buffer* buf);
  static gfx::Point ConvertTo(const Point& input);

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

template<>
class MOJO_GEOMETRY_EXPORT TypeConverter<Size, gfx::Size> {
 public:
  static Size ConvertFrom(const gfx::Size& input, Buffer* buf);
  static gfx::Size ConvertTo(const Size& input);

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

template<>
class MOJO_GEOMETRY_EXPORT TypeConverter<Rect, gfx::Rect> {
 public:
  static Rect ConvertFrom(const gfx::Rect& input, Buffer* buf);
  static gfx::Rect ConvertTo(const Rect& input);

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_
