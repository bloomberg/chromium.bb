// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "cc/output/filter_operations.h"

#include "base/values.h"
#include "cc/output/filter_operation.h"

namespace cc {

FilterOperations::FilterOperations() {}

FilterOperations::FilterOperations(const FilterOperations& other)
    : operations_(other.operations_) {}

FilterOperations::~FilterOperations() {}

FilterOperations& FilterOperations::operator=(const FilterOperations& other) {
  operations_ = other.operations_;
  return *this;
}

bool FilterOperations::operator==(const FilterOperations& other) const {
  if (other.size() != size())
    return false;
  for (size_t i = 0; i < size(); ++i) {
    if (other.at(i) != at(i))
      return false;
  }
  return true;
}

void FilterOperations::Append(const FilterOperation& filter) {
  operations_.push_back(filter);
}

void FilterOperations::Clear() {
  operations_.clear();
}

bool FilterOperations::IsEmpty() const {
  return operations_.empty();
}

static int SpreadForStdDeviation(float std_deviation) {
  // https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#feGaussianBlurElement
  // provides this approximation for evaluating a gaussian blur by a triple box
  // filter.
  float d = floorf(std_deviation * 3.f * sqrt(8.f * atan(1.f)) / 4.f + 0.5f);
  return static_cast<int>(ceilf(d * 3.f / 2.f));
}

void FilterOperations::GetOutsets(int* top,
                                  int* right,
                                  int* bottom,
                                  int* left) const {
  *top = *right = *bottom = *left = 0;
  for (size_t i = 0; i < operations_.size(); ++i) {
    const FilterOperation op = operations_[i];
    if (op.type() == FilterOperation::BLUR ||
        op.type() == FilterOperation::DROP_SHADOW) {
      int spread = SpreadForStdDeviation(op.amount());
      if (op.type() == FilterOperation::BLUR) {
        *top += spread;
        *right += spread;
        *bottom += spread;
        *left += spread;
      } else {
        *top += spread - op.drop_shadow_offset().y();
        *right += spread + op.drop_shadow_offset().x();
        *bottom += spread + op.drop_shadow_offset().y();
        *left += spread - op.drop_shadow_offset().x();
      }
    }
  }
}

bool FilterOperations::HasFilterThatMovesPixels() const {
  for (size_t i = 0; i < operations_.size(); ++i) {
    const FilterOperation op = operations_[i];
    switch (op.type()) {
      case FilterOperation::BLUR:
      case FilterOperation::DROP_SHADOW:
      case FilterOperation::ZOOM:
        return true;
      default:
        break;
    }
  }
  return false;
}

bool FilterOperations::HasFilterThatAffectsOpacity() const {
  for (size_t i = 0; i < operations_.size(); ++i) {
    const FilterOperation op = operations_[i];
    switch (op.type()) {
      case FilterOperation::OPACITY:
      case FilterOperation::BLUR:
      case FilterOperation::DROP_SHADOW:
      case FilterOperation::ZOOM:
        return true;
      case FilterOperation::COLOR_MATRIX: {
        const SkScalar* matrix = op.matrix();
        return matrix[15] || matrix[16] || matrix[17] || matrix[18] != 1 ||
               matrix[19];
      }
      default:
        break;
    }
  }
  return false;
}

FilterOperations FilterOperations::Blend(const FilterOperations& from,
                                         double progress) const {
  FilterOperations blended_filters;
  if (from.size() == 0) {
    for (size_t i = 0; i < size(); i++)
      blended_filters.Append(FilterOperation::Blend(NULL, &at(i), progress));
    return blended_filters;
  }

  if (size() == 0) {
    for (size_t i = 0; i < from.size(); i++) {
      blended_filters.Append(
          FilterOperation::Blend(&from.at(i), NULL, progress));
    }
    return blended_filters;
  }

  if (from.size() != size())
    return *this;

  for (size_t i = 0; i < size(); i++) {
    if (from.at(i).type() != at(i).type())
      return *this;
  }

  for (size_t i = 0; i < size(); i++) {
    blended_filters.Append(
        FilterOperation::Blend(&from.at(i), &at(i), progress));
  }

  return blended_filters;
}

scoped_ptr<base::Value> FilterOperations::AsValue() const {
  scoped_ptr<base::ListValue> value(new ListValue);
  for (size_t i = 0; i < operations_.size(); ++i)
    value->Append(operations_[i].AsValue().release());
  return value.PassAs<base::Value>();
}

}  // namespace cc
