// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/size_constraints.h"

#include <algorithm>

namespace apps {

SizeConstraints::SizeConstraints()
    : maximum_size_(kUnboundedSize, kUnboundedSize) {}

SizeConstraints::SizeConstraints(const gfx::Size& min_size,
                                 const gfx::Size& max_size)
    : minimum_size_(min_size), maximum_size_(max_size) {}

SizeConstraints::~SizeConstraints() {}

gfx::Size SizeConstraints::ClampSize(gfx::Size size) const {
  const gfx::Size max_size = GetMaximumSize();
  if (max_size.width() != kUnboundedSize)
    size.set_width(std::min(size.width(), GetMaximumSize().width()));
  if (max_size.height() != kUnboundedSize)
    size.set_height(std::min(size.height(), GetMaximumSize().height()));
  size.SetToMax(GetMinimumSize());
  return size;
}

bool SizeConstraints::HasMinimumSize() const {
  return GetMinimumSize().width() != kUnboundedSize ||
         GetMinimumSize().height() != kUnboundedSize;
}

bool SizeConstraints::HasMaximumSize() const {
  const gfx::Size max_size = GetMaximumSize();
  return max_size.width() != kUnboundedSize ||
         max_size.height() != kUnboundedSize;
}

bool SizeConstraints::HasFixedSize() const {
  return !GetMinimumSize().IsEmpty() && GetMinimumSize() == GetMaximumSize();
}

gfx::Size SizeConstraints::GetMinimumSize() const {
  return minimum_size_;
}

gfx::Size SizeConstraints::GetMaximumSize() const {
  return gfx::Size(
      maximum_size_.width() == kUnboundedSize
          ? kUnboundedSize
          : std::max(maximum_size_.width(), minimum_size_.width()),
      maximum_size_.height() == kUnboundedSize
          ? kUnboundedSize
          : std::max(maximum_size_.height(), minimum_size_.height()));
}

void SizeConstraints::set_minimum_size(const gfx::Size& min_size) {
  minimum_size_ = min_size;
}

void SizeConstraints::set_maximum_size(const gfx::Size& max_size) {
  maximum_size_ = max_size;
}

}  // namespace apps
