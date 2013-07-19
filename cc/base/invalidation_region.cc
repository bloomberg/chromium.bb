// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/invalidation_region.h"

#include "base/metrics/histogram.h"

namespace {

const int kMaxInvalidationRectCount = 256;

}  // namespace

namespace cc {

InvalidationRegion::InvalidationRegion() {}

InvalidationRegion::~InvalidationRegion() {}

void InvalidationRegion::Swap(Region* region) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Renderer4.InvalidationRegionApproximateRectCount",
      region_.GetRegionComplexity(),
      1,
      5000,
      50);

  SimplifyIfNeeded();
  region_.Swap(region);
}

void InvalidationRegion::Clear() {
  region_.Clear();
}

void InvalidationRegion::Union(gfx::Rect rect) {
  // TODO(vmpstr): We should simplify the region after Union() after we get a
  // good idea of what kind of regions are typical (from the UMA histogram).
  region_.Union(rect);
}

void InvalidationRegion::SimplifyIfNeeded() {
  if (region_.GetRegionComplexity() > kMaxInvalidationRectCount)
    region_ = region_.bounds();
}

}  // namespace cc
