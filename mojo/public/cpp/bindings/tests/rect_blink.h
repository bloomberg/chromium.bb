// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_BLINK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_BLINK_H_

#include "base/logging.h"
#include "mojo/public/interfaces/bindings/tests/rect.mojom.h"

namespace mojo {
namespace test {

// An implementation of a hypothetical Rect type specifically for consumers in
// in Blink. Unlike the Chromium variant (see rect_chromium.h) this does not
// support negative origin coordinates and is not copyable.
class RectBlink {
 public:
  RectBlink() {}
  RectBlink(int x, int y, int width, int height) :
      x_(x), y_(y), width_(width), height_(height) {
    DCHECK_GE(x_, 0);
    DCHECK_GE(y_, 0);
    DCHECK_GE(width_, 0);
    DCHECK_GE(height_, 0);
  }
  ~RectBlink() {}

  int x() const { return x_; }
  void setX(int x) {
    DCHECK_GE(x, 0);
    x_ = x;
  }

  int y() const { return y_; }
  void setY(int y) {
    DCHECK_GE(y, 0);
    y_ = y;
  }

  int width() const { return width_; }
  void setWidth(int width) {
    DCHECK_GE(width, 0);
    width_ = width;
  }

  int height() const { return height_; }
  void setHeight(int height) {
    DCHECK_GE(height, 0);
    height_ = height;
  }

  int computeArea() const { return width_ * height_; }

 private:
  int x_ = 0;
  int y_ = 0;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace test

template <>
struct StructTraits<test::Rect, test::RectBlink> {
  static int x(const test::RectBlink& r) { return r.x(); }
  static int y(const test::RectBlink& r) { return r.y(); }
  static int width(const test::RectBlink& r) { return r.width(); }
  static int height(const test::RectBlink& r) { return r.height(); }

  static bool Read(test::Rect::Reader r, test::RectBlink* out) {
    if (r.x() < 0 || r.y() < 0 || r.width() < 0 || r.height() < 0) {
      return false;
    }
    out->setX(r.x());
    out->setY(r.y());
    out->setWidth(r.width());
    out->setHeight(r.height());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_BLINK_H_
