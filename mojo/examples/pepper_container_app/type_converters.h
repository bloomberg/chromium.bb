// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"

namespace mojo {

template <>
class TypeConverter<Point, PP_Point> {
 public:
  static Point ConvertFrom(const PP_Point& input, Buffer* buf) {
    Point::Builder point(buf);
    point.set_x(input.x);
    point.set_y(input.y);
    return point.Finish();
  }

  static PP_Point ConvertTo(const Point& input) {
    return PP_MakePoint(static_cast<int32_t>(input.x()),
                        static_cast<int32_t>(input.y()));
  }

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

template <>
class TypeConverter<Size, PP_Size> {
 public:
  static Size ConvertFrom(const PP_Size& input, Buffer* buf) {
    Size::Builder size(buf);
    size.set_width(input.width);
    size.set_height(input.height);
    return size.Finish();
  }

  static PP_Size ConvertTo(const Size& input) {
    return PP_MakeSize(static_cast<int32_t>(input.width()),
                       static_cast<int32_t>(input.height()));
  }

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

template <>
class TypeConverter<Rect, PP_Rect> {
 public:
  static Rect ConvertFrom(const PP_Rect& input, Buffer* buf) {
    Rect::Builder rect(buf);
    rect.set_position(input.point);
    rect.set_size(input.size);
    return rect.Finish();
  }

  static PP_Rect ConvertTo(const Rect& input) {
    PP_Rect rect = { input.position().To<PP_Point>(),
                     input.size().To<PP_Size>() };
    return rect;
  }

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_
