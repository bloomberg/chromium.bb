// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/converters/transform/transform_type_converters.h"

#include <utility>

namespace mojo {

// static
TransformPtr TypeConverter<TransformPtr, gfx::Transform>::Convert(
    const gfx::Transform& input) {
  std::vector<float> storage(16);
  input.matrix().asRowMajorf(&storage[0]);
  mojo::Array<float> matrix;
  matrix.Swap(&storage);
  TransformPtr transform(Transform::New());
  transform->matrix = std::move(matrix);
  return transform;
}

// static
gfx::Transform TypeConverter<gfx::Transform, TransformPtr>::Convert(
    const TransformPtr& input) {
  if (input.is_null())
    return gfx::Transform();
  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix().setRowMajorf(&input->matrix.storage()[0]);
  return transform;
}

}  // namespace mojo
