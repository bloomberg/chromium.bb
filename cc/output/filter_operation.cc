// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/filter_operation.h"

namespace cc {

bool FilterOperation::operator==(const FilterOperation& other) const {
  if (type_ != other.type_)
    return false;
  if (type_ == COLOR_MATRIX)
    return !memcmp(matrix_, other.matrix_, sizeof(matrix_));
  if (type_ == DROP_SHADOW) {
    return amount_ == other.amount_ &&
           drop_shadow_offset_ == other.drop_shadow_offset_ &&
           drop_shadow_color_ == other.drop_shadow_color_;
  }
  return amount_ == other.amount_;
}

FilterOperation::FilterOperation(FilterType type, float amount)
    : type_(type),
      amount_(amount),
      drop_shadow_offset_(0, 0),
      drop_shadow_color_(0),
      zoom_inset_(0) {
  DCHECK_NE(type_, DROP_SHADOW);
  DCHECK_NE(type_, COLOR_MATRIX);
  memset(matrix_, 0, sizeof(matrix_));
}

FilterOperation::FilterOperation(FilterType type,
                                 gfx::Point offset,
                                 float stdDeviation,
                                 SkColor color)
    : type_(type),
      amount_(stdDeviation),
      drop_shadow_offset_(offset),
      drop_shadow_color_(color),
      zoom_inset_(0) {
  DCHECK_EQ(type_, DROP_SHADOW);
  memset(matrix_, 0, sizeof(matrix_));
}

FilterOperation::FilterOperation(FilterType type, SkScalar matrix[20])
    : type_(type),
      amount_(0),
      drop_shadow_offset_(0, 0),
      drop_shadow_color_(0),
      zoom_inset_(0) {
  DCHECK_EQ(type_, COLOR_MATRIX);
  memcpy(matrix_, matrix, sizeof(matrix_));
}

FilterOperation::FilterOperation(FilterType type, float amount, int inset)
    : type_(type),
      amount_(amount),
      drop_shadow_offset_(0, 0),
      drop_shadow_color_(0),
      zoom_inset_(inset) {
  DCHECK_EQ(type_, ZOOM);
  memset(matrix_, 0, sizeof(matrix_));
}

}  // namespace cc
