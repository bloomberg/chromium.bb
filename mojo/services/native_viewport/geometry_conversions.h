// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_GEOMETRY_CONVERSIONS_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_GEOMETRY_CONVERSIONS_H_

#include "mojom/native_viewport.h"
#include "ui/gfx/rect.h"

namespace mojo {

template<>
class TypeConverter<Point, gfx::Point> {
public:
  static Point ConvertFrom(const gfx::Point& input, Buffer* buf) {
    Point::Builder point(buf);
    point.set_x(input.x());
    point.set_y(input.y());
    return point.Finish();
  }
  static gfx::Point ConvertTo(const Point& input) {
    return gfx::Point(input.x(), input.y());
  }
};

template<>
class TypeConverter<Size, gfx::Size> {
public:
  static Size ConvertFrom(const gfx::Size& input, Buffer* buf) {
    Size::Builder size(buf);
    size.set_width(input.width());
    size.set_height(input.height());
    return size.Finish();
  }
  static gfx::Size ConvertTo(const Size& input) {
    return gfx::Size(input.width(), input.height());
  }
};

template<>
class TypeConverter<Rect, gfx::Rect> {
 public:
  static Rect ConvertFrom(const gfx::Rect& input, Buffer* buf) {
    Rect::Builder rect(buf);
    rect.set_position(input.origin());
    rect.set_size(input.size());
    return rect.Finish();
  }
  static gfx::Rect ConvertTo(const Rect& input) {
    return gfx::Rect(input.position().x(), input.position().y(),
                     input.size().width(), input.size().height());
  }
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_GEOMETRY_CONVERSIONS_H_
