// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/magnetism_matcher.h"

#include <algorithm>
#include <cmath>

namespace ash {
namespace internal {

// MagnetismEdgeMatcher --------------------------------------------------------

MagnetismEdgeMatcher::MagnetismEdgeMatcher(const gfx::Rect& bounds,
                                           MagnetismEdge edge)
    : bounds_(bounds),
      edge_(edge) {
  ranges_.push_back(GetSecondaryRange(bounds_));
}

MagnetismEdgeMatcher::~MagnetismEdgeMatcher() {
}

bool MagnetismEdgeMatcher::ShouldAttach(const gfx::Rect& bounds) {
  if (is_edge_obscured())
    return false;

  if (abs(GetPrimaryCoordinate(bounds_, edge_) -
          GetPrimaryCoordinate(bounds, FlipEdge(edge_))) <=
      MagnetismMatcher::kMagneticDistance) {
    const Range range(GetSecondaryRange(bounds));
    Ranges::const_iterator i =
        std::lower_bound(ranges_.begin(), ranges_.end(), range);
    // Close enough, but only attach if some portion of the edge is visible.
    if ((i != ranges_.begin() && RangesIntersect(*(i - 1), range)) ||
        (i != ranges_.end() && RangesIntersect(*i, range))) {
      return true;
    }
  }
  // NOTE: this checks against the current bounds, we may want to allow some
  // flexibility here.
  const Range primary_range(GetPrimaryRange(bounds));
  if (primary_range.first <= GetPrimaryCoordinate(bounds_, edge_) &&
      primary_range.second >= GetPrimaryCoordinate(bounds_, edge_)) {
    UpdateRanges(GetSecondaryRange(bounds));
  }
  return false;
}

void MagnetismEdgeMatcher::UpdateRanges(const Range& range) {
  Ranges::const_iterator it =
      std::lower_bound(ranges_.begin(), ranges_.end(), range);
  if (it != ranges_.begin() && RangesIntersect(*(it - 1), range))
    --it;
  if (it == ranges_.end())
    return;

  for (size_t i = it - ranges_.begin();
       i < ranges_.size() && RangesIntersect(ranges_[i], range); ) {
    if (range.first <= ranges_[i].first &&
        range.second >= ranges_[i].second) {
      ranges_.erase(ranges_.begin() + i);
    } else if (range.first < ranges_[i].first) {
      DCHECK_GT(range.second, ranges_[i].first);
      ranges_[i] = Range(range.second, ranges_[i].second);
      ++i;
    } else {
      Range existing(ranges_[i]);
      ranges_[i].second = range.first;
      ++i;
      if (existing.second > range.second) {
        ranges_.insert(ranges_.begin() + i,
                       Range(range.second, existing.second));
        ++i;
      }
    }
  }
}

// MagnetismMatcher ------------------------------------------------------------

// static
const int MagnetismMatcher::kMagneticDistance = 8;

MagnetismMatcher::MagnetismMatcher(const gfx::Rect& bounds) {
  matchers_.push_back(new MagnetismEdgeMatcher(bounds, MAGNETISM_EDGE_TOP));
  matchers_.push_back(new MagnetismEdgeMatcher(bounds, MAGNETISM_EDGE_LEFT));
  matchers_.push_back(new MagnetismEdgeMatcher(bounds, MAGNETISM_EDGE_BOTTOM));
  matchers_.push_back(new MagnetismEdgeMatcher(bounds, MAGNETISM_EDGE_RIGHT));
}

MagnetismMatcher::~MagnetismMatcher() {
}

// static
bool MagnetismMatcher::ShouldAttachOnEdge(const gfx::Rect& src_bounds,
                                          const gfx::Rect& bounds,
                                          MagnetismEdge edge) {
  MagnetismEdgeMatcher edge_matcher(src_bounds, edge);
  return edge_matcher.ShouldAttach(bounds);
}

bool MagnetismMatcher::ShouldAttach(const gfx::Rect& bounds,
                                    MagnetismEdge* edge) {
  for (size_t i = 0; i < matchers_.size(); ++i) {
    if (matchers_[i]->ShouldAttach(bounds)) {
      *edge = matchers_[i]->edge();
      return true;
    }
  }
  return false;
}

bool MagnetismMatcher::AreEdgesObscured() const {
  for (size_t i = 0; i < matchers_.size(); ++i) {
    if (!matchers_[i]->is_edge_obscured())
      return false;
  }
  return true;
}


}  // namespace internal
}  // namespace ash
