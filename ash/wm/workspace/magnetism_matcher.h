// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_MAGNETISM_MATCHER_H_
#define ASH_WM_WORKSPACE_MAGNETISM_MATCHER_H_

#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace internal {

enum MagnetismEdge {
  MAGNETISM_EDGE_TOP,
  MAGNETISM_EDGE_LEFT,
  MAGNETISM_EDGE_BOTTOM,
  MAGNETISM_EDGE_RIGHT,
};

// MagnetismEdgeMatcher is used for matching a particular edge of a window. You
// shouldn't need to use this directly, instead use MagnetismMatcher which takes
// care of all edges.
// MagnetismEdgeMatcher maintains a range of the visible portions of the
// edge. As ShouldAttach() is invoked the visible range is updated.
class MagnetismEdgeMatcher {
 public:
  MagnetismEdgeMatcher(const gfx::Rect& bounds, MagnetismEdge edge);
  ~MagnetismEdgeMatcher();

  MagnetismEdge edge() const { return edge_; }

  // Returns true if the edge is completely obscured. If true ShouldAttach()
  // will return false.
  bool is_edge_obscured() const { return ranges_.empty(); }

  // Returns true if should attach to the specified bounds.
  bool ShouldAttach(const gfx::Rect& bounds);

 private:
  typedef std::pair<int,int> Range;
  typedef std::vector<Range> Ranges;

  // Removes |range| from |ranges_|.
  void UpdateRanges(const Range& range);

  static int GetPrimaryCoordinate(const gfx::Rect& bounds, MagnetismEdge edge) {
    switch (edge) {
      case MAGNETISM_EDGE_TOP:
        return bounds.y();
      case MAGNETISM_EDGE_LEFT:
        return bounds.x();
      case MAGNETISM_EDGE_BOTTOM:
        return bounds.bottom();
      case MAGNETISM_EDGE_RIGHT:
        return bounds.right();
    }
    NOTREACHED();
    return 0;
  }

  static MagnetismEdge FlipEdge(MagnetismEdge edge) {
    switch (edge) {
      case MAGNETISM_EDGE_TOP:
        return MAGNETISM_EDGE_BOTTOM;
      case MAGNETISM_EDGE_BOTTOM:
        return MAGNETISM_EDGE_TOP;
      case MAGNETISM_EDGE_LEFT:
        return MAGNETISM_EDGE_RIGHT;
      case MAGNETISM_EDGE_RIGHT:
        return MAGNETISM_EDGE_LEFT;
    }
    NOTREACHED();
    return MAGNETISM_EDGE_LEFT;
  }

  Range GetPrimaryRange(const gfx::Rect& bounds) const {
    switch (edge_) {
      case MAGNETISM_EDGE_TOP:
      case MAGNETISM_EDGE_BOTTOM:
        return Range(bounds.y(), bounds.bottom());
      case MAGNETISM_EDGE_LEFT:
      case MAGNETISM_EDGE_RIGHT:
        return Range(bounds.x(), bounds.right());
    }
    NOTREACHED();
    return Range();
  }

  Range GetSecondaryRange(const gfx::Rect& bounds) const {
    switch (edge_) {
      case MAGNETISM_EDGE_TOP:
      case MAGNETISM_EDGE_BOTTOM:
        return Range(bounds.x(), bounds.right());
      case MAGNETISM_EDGE_LEFT:
      case MAGNETISM_EDGE_RIGHT:
        return Range(bounds.y(), bounds.bottom());
    }
    NOTREACHED();
    return Range();
  }

  static bool RangesIntersect(const Range& r1, const Range& r2) {
    return r2.first < r1.second && r2.second > r1.first;
  }

  // The bounds of window.
  const gfx::Rect bounds_;

  // The edge this matcher checks.
  const MagnetismEdge edge_;

  // Visible ranges of the edge. Initialized with GetSecondaryRange() and
  // updated as ShouldAttach() is invoked. When empty the edge is completely
  // obscured by other bounds.
  Ranges ranges_;

  DISALLOW_COPY_AND_ASSIGN(MagnetismEdgeMatcher);
};

// MagnetismMatcher is used to test if a window should snap to another window.
// To use MagnetismMatcher do the following:
// . Create it with the bounds of the window being dragged.
// . Iterate over the child windows checking if the window being dragged should
//   attach to it using ShouldAttach().
// . Use AreEdgesObscured() to test if no other windows can match (because all
//   edges are completely obscured).
class ASH_EXPORT MagnetismMatcher {
 public:
  static const int kMagneticDistance;

  explicit MagnetismMatcher(const gfx::Rect& bounds);
  ~MagnetismMatcher();

  // Checks a single edge. |src_bounds| is the bounds of the window being
  // dragged, |bounds| the bounds attempting to snap to and |edge| the edge to
  // check.
  static bool ShouldAttachOnEdge(const gfx::Rect& src_bounds,
                                 const gfx::Rect& bounds,
                                 MagnetismEdge edge);

  // Returns true if |bounds| is close enough to the initial bounds that the two
  // should be attached. If true is returned |edge| is set to the edge the two
  // should be attached to. |edge| is in terms of the initial bounds. For
  // example, if this returns true and |edge| is set to MAGNETISM_EDGE_RIGHT
  // then the right edge of the source should snap to the left edge of |bounds|.
  bool ShouldAttach(const gfx::Rect& bounds, MagnetismEdge* edge);

  // Returns true if no other matches are possible.
  bool AreEdgesObscured() const;

 private:
  ScopedVector<MagnetismEdgeMatcher> matchers_;

  DISALLOW_COPY_AND_ASSIGN(MagnetismMatcher);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_MAGNETISM_MATCHER_H_
