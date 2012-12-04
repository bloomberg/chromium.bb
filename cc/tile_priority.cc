// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile_priority.h"

namespace {

// TODO(qinmin): modify ui/range/Range.h to support template so that we
// don't need to define this.
struct Range {
  Range(double start, double end) : start_(start), end_(end) {}
  Range Intersects(const Range& other);
  bool IsEmpty();
  double start_;
  double end_;
};

Range Range::Intersects(const Range& other) {
  start_ = std::max(start_, other.start_);
  end_ = std::min(end_, other.end_);
  return Range(start_, end_);
}

bool Range::IsEmpty() {
  return start_ >= end_;
}

// Calculate a time range that |value| will be larger than |threshold|
// given the velocity of its change.
Range TimeRangeValueLargerThanThreshold(
    int value, int threshold, double velocity) {
  double minimum_time = 0;
  double maximum_time = cc::TilePriority::kMaxTimeToVisibleInSeconds;

  if (velocity > 0) {
    if (value < threshold)
      minimum_time = std::min(cc::TilePriority::kMaxTimeToVisibleInSeconds,
                              (threshold - value) / velocity);
  } else if (velocity <= 0) {
    if (value < threshold)
      minimum_time = cc::TilePriority::kMaxTimeToVisibleInSeconds;
    else if (velocity != 0)
      maximum_time = std::min(maximum_time, (threshold - value) / velocity);
  }

  return Range(minimum_time, maximum_time);
}

}  // namespace

namespace cc {

const double TilePriority::kMaxTimeToVisibleInSeconds = 1000;

int TilePriority::manhattanDistance(const gfx::RectF& a, const gfx::RectF& b) {
  gfx::RectF c = gfx::UnionRects(a, b);
  // Rects touching the edge of the screen should not be considered visible.
  // So we add 1 pixel here to avoid that situation.
  int x = static_cast<int>(
      std::max(0.0f, c.width() - a.width() - b.width() + 1));
  int y = static_cast<int>(
      std::max(0.0f, c.height() - a.height() - b.height() + 1));
  return (x + y);
}

double TilePriority::TimeForBoundsToIntersect(gfx::RectF previous_bounds,
                                              gfx::RectF current_bounds,
                                              double time_delta,
                                              gfx::RectF target_bounds) {
  if (current_bounds.Intersects(target_bounds))
    return 0;

  if (previous_bounds.Intersects(target_bounds) || time_delta == 0)
    return kMaxTimeToVisibleInSeconds;

  // As we are trying to solve the case of both scaling and scrolling, using
  // a single coordinate with velocity is not enough. The logic here is to
  // calculate the velocity for each edge. Then we calculate the time range that
  // each edge will stay on the same side of the target bounds. If there is an
  // overlap between these time ranges, the bounds must have intersect with
  // each other during that period of time.
  double velocity =
      (current_bounds.right() - previous_bounds.right()) / time_delta;
  Range range = TimeRangeValueLargerThanThreshold(
      current_bounds.right(), target_bounds.x(), velocity);

  velocity = (current_bounds.x() - previous_bounds.x()) / time_delta;
  range = range.Intersects(TimeRangeValueLargerThanThreshold(
      -current_bounds.x(), -target_bounds.right(), -velocity));


  velocity = (current_bounds.y() - previous_bounds.y()) / time_delta;
  range = range.Intersects(TimeRangeValueLargerThanThreshold(
      -current_bounds.y(), -target_bounds.bottom(), -velocity));

  velocity = (current_bounds.bottom() - previous_bounds.bottom()) / time_delta;
  range = range.Intersects(TimeRangeValueLargerThanThreshold(
      current_bounds.bottom(), target_bounds.y(), velocity));

  return range.IsEmpty() ? kMaxTimeToVisibleInSeconds : range.start_;
}

}  // namespace cc
