// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"

namespace mojo {

template <>
struct TypeConverter<PointPtr, PP_Point> {
  static PointPtr Convert(const PP_Point& input) {
    PointPtr point(Point::New());
    point->x = input.x;
    point->y = input.y;
    return point.Pass();
  }
};

template <>
struct TypeConverter<PP_Point, PointPtr> {
  static PP_Point Convert(const PointPtr& input) {
    if (!input)
      return PP_MakePoint(0, 0);
    return PP_MakePoint(static_cast<int32_t>(input->x),
                        static_cast<int32_t>(input->y));
  }
};

template <>
struct TypeConverter<SizePtr, PP_Size> {
  static SizePtr Convert(const PP_Size& input) {
    SizePtr size(Size::New());
    size->width = input.width;
    size->height = input.height;
    return size.Pass();
  }
};

template <>
struct TypeConverter<PP_Size, SizePtr> {
  static PP_Size Convert(const SizePtr& input) {
    if (!input)
      return PP_MakeSize(0, 0);
    return PP_MakeSize(static_cast<int32_t>(input->width),
                       static_cast<int32_t>(input->height));
  }
};

template <>
struct TypeConverter<RectPtr, PP_Rect> {
  static RectPtr Convert(const PP_Rect& input) {
    RectPtr rect(Rect::New());
    rect->x = input.point.x;
    rect->y = input.point.y;
    rect->width = input.size.width;
    rect->height = input.size.height;
    return rect.Pass();
  }
};

template <>
struct TypeConverter<PP_Rect, SizePtr> {
  static PP_Rect Convert(const SizePtr& input) {
    if (!input)
      return PP_MakeRectFromXYWH(0, 0, 0, 0);
    return PP_MakeRectFromXYWH(0, 0, input->width, input->height);
  }
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_
