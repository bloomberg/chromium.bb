// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_

#include "mojo/services/public/cpp/geometry/mojo_geometry_export.h"
#include "mojo/services/public/interfaces/geometry/geometry.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace mojo {

template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<PointPtr, gfx::Point> {
  static PointPtr Convert(const gfx::Point& input);
};
template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<gfx::Point, PointPtr> {
  static gfx::Point Convert(const PointPtr& input);
};

template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<PointFPtr, gfx::PointF> {
  static PointFPtr Convert(const gfx::PointF& input);
};
template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<gfx::PointF, PointFPtr> {
  static gfx::PointF Convert(const PointFPtr& input);
};

template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<SizePtr, gfx::Size> {
  static SizePtr Convert(const gfx::Size& input);
};
template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<gfx::Size, SizePtr> {
  static gfx::Size Convert(const SizePtr& input);
};

template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<RectPtr, gfx::Rect> {
  static RectPtr Convert(const gfx::Rect& input);
};
template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<gfx::Rect, RectPtr> {
  static gfx::Rect Convert(const RectPtr& input);
};

template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<RectFPtr, gfx::RectF> {
  static RectFPtr Convert(const gfx::RectF& input);
};
template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<gfx::RectF, RectFPtr> {
  static gfx::RectF Convert(const RectFPtr& input);
};

template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<TransformPtr, gfx::Transform> {
  static TransformPtr Convert(const gfx::Transform& input);
};
template <>
struct MOJO_GEOMETRY_EXPORT TypeConverter<gfx::Transform, TransformPtr> {
  static gfx::Transform Convert(const TransformPtr& input);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_
