// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CONVERTERS_TRANSFORM_TRANSFORM_TYPE_CONVERTERS_H_
#define MOJO_CONVERTERS_TRANSFORM_TRANSFORM_TYPE_CONVERTERS_H_

#include "mojo/converters/transform/mojo_transform_export.h"
#include "ui/gfx/transform.h"
#include "ui/mojo/geometry/geometry.mojom.h"

namespace mojo {

template <>
struct MOJO_TRANSFORM_EXPORT TypeConverter<TransformPtr, gfx::Transform> {
  static TransformPtr Convert(const gfx::Transform& input);
};
template <>
struct MOJO_TRANSFORM_EXPORT TypeConverter<gfx::Transform, TransformPtr> {
  static gfx::Transform Convert(const TransformPtr& input);
};

}  // namespace mojo

#endif  // MOJO_CONVERTERS_TRANSFORM_TRANSFORM_TYPE_CONVERTERS_H_
