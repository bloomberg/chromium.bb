// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_priority.h"

#include "base/values.h"
#include "cc/base/math_util.h"

namespace {

// TODO(qinmin): modify ui/range/Range.h to support template so that we
// don't need to define this.
struct Range {
  Range(float start, float end) : start_(start), end_(end) {}
  bool IsEmpty();
  float start_;
  float end_;
};

inline bool Intersects(const Range& a, const Range& b) {
  return a.start_ < b.end_ && b.start_ < a.end_;
}

inline Range Intersect(const Range& a, const Range& b) {
  return Range(std::max(a.start_, b.start_), std::min(a.end_, b.end_));
}

bool Range::IsEmpty() {
  return start_ >= end_;
}

inline void IntersectNegativeHalfplane(Range* out,
                                       float previous,
                                       float current,
                                       float target,
                                       float time_delta) {
  float time_per_dist = time_delta / (current - previous);
  float t = (target - current) * time_per_dist;
  if (time_per_dist > 0.0f)
    out->start_ = std::max(out->start_, t);
  else
    out->end_ = std::min(out->end_, t);
}

inline void IntersectPositiveHalfplane(Range* out,
                                       float previous,
                                       float current,
                                       float target,
                                       float time_delta) {
  float time_per_dist = time_delta / (current - previous);
  float t = (target - current) * time_per_dist;
  if (time_per_dist < 0.0f)
    out->start_ = std::max(out->start_, t);
  else
    out->end_ = std::min(out->end_, t);
}

}  // namespace

namespace cc {

scoped_ptr<base::Value> WhichTreeAsValue(WhichTree tree) {
  switch (tree) {
  case ACTIVE_TREE:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "ACTIVE_TREE"));
  case PENDING_TREE:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "PENDING_TREE"));
  default:
      DCHECK(false) << "Unrecognized WhichTree value " << tree;
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown WhichTree value>"));
  }
}

scoped_ptr<base::Value> TileResolutionAsValue(
    TileResolution resolution) {
  switch (resolution) {
  case LOW_RESOLUTION:
    return scoped_ptr<base::Value>(base::Value::CreateStringValue(
        "LOW_RESOLUTION"));
  case HIGH_RESOLUTION:
    return scoped_ptr<base::Value>(base::Value::CreateStringValue(
        "HIGH_RESOLUTION"));
  case NON_IDEAL_RESOLUTION:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
        "NON_IDEAL_RESOLUTION"));
  default:
    DCHECK(false) << "Unrecognized TileResolution value " << resolution;
    return scoped_ptr<base::Value>(base::Value::CreateStringValue(
        "<unknown TileResolution value>"));
  }
}

scoped_ptr<base::Value> TilePriority::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetBoolean("is_live", is_live);
  state->Set("resolution", TileResolutionAsValue(resolution).release());
  state->Set("time_to_visible_in_seconds",
             MathUtil::AsValueSafely(time_to_visible_in_seconds).release());
  state->Set("distance_to_visible_in_pixels",
             MathUtil::AsValueSafely(distance_to_visible_in_pixels).release());
  state->Set("current_screen_quad",
             MathUtil::AsValue(current_screen_quad).release());
  return state.PassAs<base::Value>();
}

float TilePriority::TimeForBoundsToIntersect(const gfx::RectF& previous_bounds,
                                             const gfx::RectF& current_bounds,
                                             float time_delta,
                                             const gfx::RectF& target_bounds) {
  // Perform an intersection test explicitly between current and target.
  if (current_bounds.x() < target_bounds.right() &&
      current_bounds.y() < target_bounds.bottom() &&
      target_bounds.x() < current_bounds.right() &&
      target_bounds.y() < current_bounds.bottom())
    return 0.0f;

  const float kMaxTimeToVisibleInSeconds =
      std::numeric_limits<float>::infinity();

  if (time_delta == 0.0f)
    return kMaxTimeToVisibleInSeconds;

  // As we are trying to solve the case of both scaling and scrolling, using
  // a single coordinate with velocity is not enough. The logic here is to
  // calculate the velocity for each edge. Then we calculate the time range that
  // each edge will stay on the same side of the target bounds. If there is an
  // overlap between these time ranges, the bounds must have intersect with
  // each other during that period of time.
  Range range(0.0f, kMaxTimeToVisibleInSeconds);
  IntersectPositiveHalfplane(
      &range, previous_bounds.x(), current_bounds.x(),
      target_bounds.right(), time_delta);
  IntersectNegativeHalfplane(
      &range, previous_bounds.right(), current_bounds.right(),
      target_bounds.x(), time_delta);
  IntersectPositiveHalfplane(
      &range, previous_bounds.y(), current_bounds.y(),
      target_bounds.bottom(), time_delta);
  IntersectNegativeHalfplane(
      &range, previous_bounds.bottom(), current_bounds.bottom(),
      target_bounds.y(), time_delta);
  return range.IsEmpty() ? kMaxTimeToVisibleInSeconds : range.start_;
}

scoped_ptr<base::Value> TileMemoryLimitPolicyAsValue(
    TileMemoryLimitPolicy policy) {
  switch (policy) {
  case ALLOW_NOTHING:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "ALLOW_NOTHING"));
  case ALLOW_ABSOLUTE_MINIMUM:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "ALLOW_ABSOLUTE_MINIMUM"));
  case ALLOW_PREPAINT_ONLY:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "ALLOW_PREPAINT_ONLY"));
  case ALLOW_ANYTHING:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "ALLOW_ANYTHING"));
  default:
      DCHECK(false) << "Unrecognized policy value";
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown>"));
  }
}

scoped_ptr<base::Value> TreePriorityAsValue(TreePriority prio) {
  switch (prio) {
  case SAME_PRIORITY_FOR_BOTH_TREES:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "SAME_PRIORITY_FOR_BOTH_TREES"));
  case SMOOTHNESS_TAKES_PRIORITY:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "SMOOTHNESS_TAKES_PRIORITY"));
  case NEW_CONTENT_TAKES_PRIORITY:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NEW_CONTENT_TAKES_PRIORITY"));
  default:
      DCHECK(false) << "Unrecognized priority value " << prio;
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown>"));
  }
}

scoped_ptr<base::Value> GlobalStateThatImpactsTilePriority::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->Set("memory_limit_policy",
             TileMemoryLimitPolicyAsValue(memory_limit_policy).release());
  state->SetInteger("memory_limit_in_bytes", memory_limit_in_bytes);
  state->Set("tree_priority", TreePriorityAsValue(tree_priority).release());
  return state.PassAs<base::Value>();
}

}  // namespace cc
