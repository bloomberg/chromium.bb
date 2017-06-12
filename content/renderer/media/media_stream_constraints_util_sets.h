// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_SETS_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_SETS_H_

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_constraints_util.h"

namespace blink {
struct WebMediaTrackConstraintSet;
}

namespace content {

// This class template represents a set of candidates suitable for a numeric
// range-based constraint.
template <typename T>
class NumericRangeSet {
 public:
  NumericRangeSet() : min_(0), max_(DefaultMax()) {}
  NumericRangeSet(T min, T max) : min_(min), max_(max) {}
  NumericRangeSet(const NumericRangeSet& other) = default;
  NumericRangeSet& operator=(const NumericRangeSet& other) = default;
  ~NumericRangeSet() = default;

  T Min() const { return min_; }
  T Max() const { return max_; }
  bool IsEmpty() const { return max_ < min_; }

  NumericRangeSet Intersection(const NumericRangeSet& other) const {
    return NumericRangeSet(std::max(min_, other.min_),
                           std::min(max_, other.max_));
  }

  // Creates a NumericRangeSet based on the minimum and maximum values of
  // |constraint|.
  template <typename ConstraintType>
  static auto FromConstraint(ConstraintType constraint)
      -> NumericRangeSet<decltype(constraint.Min())> {
    return NumericRangeSet<decltype(constraint.Min())>(
        ConstraintHasMin(constraint) ? ConstraintMin(constraint) : 0,
        ConstraintHasMax(constraint) ? ConstraintMax(constraint)
                                     : DefaultMax());
  }

 private:
  static inline T DefaultMax() {
    return std::numeric_limits<T>::has_infinity
               ? std::numeric_limits<T>::infinity()
               : std::numeric_limits<T>::max();
  }
  T min_;
  T max_;
};

// This class defines a set of discrete elements suitable for resolving
// constraints with a countable number of choices not suitable to be constrained
// by range. Examples are strings, booleans and certain constraints of type
// long. A DiscreteSet can be empty, have their elements explicitly stated, or
// be the universal set. The universal set is a set that contains all possible
// elements. The specific definition of what elements are in the universal set
// is application defined (e.g., it could be all possible boolean values, all
// possible strings of length N, or anything that suits a particular
// application).
template <typename T>
class DiscreteSet {
 public:
  // Creates a set containing the elements in |elements|.
  // It is the responsibility of the caller to ensure that |elements| is not
  // equivalent to the universal set and that |elements| has no repeated
  // values. Takes ownership of |elements|.
  explicit DiscreteSet(std::vector<T> elements)
      : is_universal_(false), elements_(std::move(elements)) {}
  // Creates an empty set;
  static DiscreteSet EmptySet() { return DiscreteSet(std::vector<T>()); }
  static DiscreteSet UniversalSet() { return DiscreteSet(); }

  DiscreteSet(const DiscreteSet& other) = default;
  DiscreteSet& operator=(const DiscreteSet& other) = default;
  DiscreteSet(DiscreteSet&& other) = default;
  DiscreteSet& operator=(DiscreteSet&& other) = default;
  ~DiscreteSet() = default;

  bool Contains(const T& value) const {
    return is_universal_ || std::find(elements_.begin(), elements_.end(),
                                      value) != elements_.end();
  }

  bool IsEmpty() const { return !is_universal_ && elements_.empty(); }

  bool HasExplicitElements() const { return !elements_.empty(); }

  DiscreteSet Intersection(const DiscreteSet& other) const {
    if (is_universal_)
      return other;
    if (other.is_universal_)
      return *this;
    if (IsEmpty() || other.IsEmpty())
      return EmptySet();

    // Both sets have explicit elements.
    std::vector<T> intersection;
    for (const auto& entry : elements_) {
      auto it =
          std::find(other.elements_.begin(), other.elements_.end(), entry);
      if (it != other.elements_.end()) {
        intersection.push_back(entry);
      }
    }
    return DiscreteSet(std::move(intersection));
  }

  // Returns a copy of the first element in the set. This is useful as a simple
  // tie-breaker rule. This applies only to constrained nonempty sets.
  // Behavior is undefined if the set is empty or universal.
  T FirstElement() const {
    DCHECK(HasExplicitElements());
    return elements_[0];
  }

  bool is_universal() const { return is_universal_; }

 private:
  // Creates a universal set.
  DiscreteSet() : is_universal_(true) {}

  bool is_universal_;
  std::vector<T> elements_;
};

// This class represents a set of (height, width) screen resolution candidates
// determined by width, height and aspect-ratio constraints.
// This class supports widths and heights from 0 to kMaxDimension, both
// inclusive and aspect ratios from 0.0 to positive infinity, both inclusive.
class CONTENT_EXPORT ResolutionSet {
 public:
  static const int kMaxDimension = std::numeric_limits<int>::max();

  // Helper class that represents (height, width) points on a plane.
  // TODO(guidou): Use a generic point/vector class that uses double once it
  // becomes available (e.g., a gfx::Vector2dD).
  class CONTENT_EXPORT Point {
   public:
    // Creates a (|height|, |width|) point. |height| and |width| must be finite.
    Point(double height, double width);
    Point(const Point& other);
    Point& operator=(const Point& other);
    ~Point();

    // Accessors.
    double height() const { return height_; }
    double width() const { return width_; }
    double AspectRatio() const { return width_ / height_; }

    // Exact equality/inequality operators.
    bool operator==(const Point& other) const;
    bool operator!=(const Point& other) const;

    // Returns true if the coordinates of this point and |other| are
    // approximately equal.
    bool IsApproximatelyEqualTo(const Point& other) const;

    // Vector-style addition and subtraction operators.
    Point operator+(const Point& other) const;
    Point operator-(const Point& other) const;

    // Returns the dot product between |p1| and |p2|.
    static double Dot(const Point& p1, const Point& p2);

    // Returns the square Euclidean distance between |p1| and |p2|.
    static double SquareEuclideanDistance(const Point& p1, const Point& p2);

    // Returns the point in the line segment determined by |s1| and |s2| that
    // is closest to |p|.
    static Point ClosestPointInSegment(const Point& p,
                                       const Point& s1,
                                       const Point& s2);

   private:
    double height_;
    double width_;
  };

  // Creates a set with the maximum supported ranges for width, height and
  // aspect ratio.
  ResolutionSet();
  ResolutionSet(int min_height,
                int max_height,
                int min_width,
                int max_width,
                double min_aspect_ratio,
                double max_aspect_ratio);
  ResolutionSet(const ResolutionSet& other);
  ResolutionSet& operator=(const ResolutionSet& other);
  ~ResolutionSet();

  // Getters.
  int min_height() const { return min_height_; }
  int max_height() const { return max_height_; }
  int min_width() const { return min_width_; }
  int max_width() const { return max_width_; }
  double min_aspect_ratio() const { return min_aspect_ratio_; }
  double max_aspect_ratio() const { return max_aspect_ratio_; }

  // Returns true if this set is empty.
  bool IsEmpty() const;

  // These functions return true if a particular variable causes the set to be
  // empty.
  bool IsHeightEmpty() const;
  bool IsWidthEmpty() const;
  bool IsAspectRatioEmpty() const;

  // These functions return true if the given point is included in this set.
  bool ContainsPoint(const Point& point) const;
  bool ContainsPoint(int height, int width) const;

  // Returns a new set with the intersection of |*this| and |other|.
  ResolutionSet Intersection(const ResolutionSet& other) const;

  // Returns a point in this (nonempty) set closest to the ideal values for the
  // height, width and aspectRatio constraints in |constraint_set|.
  // Note that this function ignores all the other data in |constraint_set|.
  // Only the ideal height, width and aspect ratio are used, and from now on
  // referred to as |ideal_height|, |ideal_width| and |ideal_aspect_ratio|
  // respectively.
  //
  // * If all three ideal values are given, |ideal_aspect_ratio| is ignored and
  //   the point closest to (|ideal_height|, |ideal_width|) is returned.
  // * If two ideal values are given, they are used to determine a single ideal
  //   point, which can be one of:
  //    - (|ideal_height|, |ideal_width|),
  //    - (|ideal_height|, |ideal_height|*|ideal_aspect_ratio|), or
  //    - (|ideal_width| / |ideal_aspect_ratio|, |ideal_width|).
  //   The point in the set closest to the ideal point is returned.
  // * If a single ideal value is given, a point in the set closest to the line
  //   defined by the ideal value is returned. If there is more than one point
  //   closest to the ideal line, the following tie-breaker rules are used:
  //  - If |ideal_height| is provided, the point closest to
  //      (|ideal_height|, |ideal_height| * default_aspect_ratio), where
  //      default_aspect_ratio is the result of the floating-point division
  //      |default_width|/|default_height|.
  //  - If |ideal_width| is provided, the point closest to
  //      (|ideal_width| / default_aspect_ratio, |ideal_width|).
  //  - If |ideal_aspect_ratio| is provided, the point with largest area among
  //    the points closest to
  //      (|default_height|, |default_height| * aspect_ratio) and
  //      (|default_width| / aspect_ratio, |default_width|),
  //    where aspect_ratio is |ideal_aspect_ratio| if the ideal line intersects
  //    the set, and the closest aspect ratio to |ideal_aspect_ratio| among the
  //    points in the set if no point in the set has an aspect ratio equal to
  //    |ideal_aspect_ratio|.
  // * If no ideal value is given, proceed as if only |ideal_aspect_ratio| was
  //   provided with a value of default_aspect_ratio.
  //
  // This is intended to support the implementation of the spec algorithm for
  // selection of track settings, as defined in
  // https://w3c.github.io/mediacapture-main/#dfn-selectsettings.
  //
  // The main difference between this algorithm and the spec is that when ideal
  // values are provided, the spec mandates finding a point that minimizes the
  // sum of custom relative distances for each provided ideal value, while this
  // algorithm minimizes either the Euclidean distance (sum of square distances)
  // on a height-width plane for the cases where two or three ideal values are
  // provided, or the absolute distance for the case with one ideal value.
  // Also, in the case with three ideal values, this algorithm ignores the
  // distance to the ideal aspect ratio.
  // In most cases the difference in the final result should be negligible.
  // The reason to follow this approach is that optimization in the worst case
  // is reduced to projection of a point on line segment, which is a simple
  // operation that provides exact results. Using the distance function of the
  // spec, which is not continuous, would require complex optimization methods
  // that do not necessarily guarantee finding the real optimal value.
  //
  // This function has undefined behavior if this set is empty.
  Point SelectClosestPointToIdeal(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      int default_height,
      int default_width) const;

  // Utilities that return ResolutionSets constrained on a specific variable.
  static ResolutionSet FromHeight(int min, int max);
  static ResolutionSet FromExactHeight(int value);
  static ResolutionSet FromWidth(int min, int max);
  static ResolutionSet FromExactWidth(int value);
  static ResolutionSet FromAspectRatio(double min, double max);
  static ResolutionSet FromExactAspectRatio(double value);

  // Returns a ResolutionCandidateSet initialized with |constraint_set|'s
  // width, height and aspectRatio constraints.
  static ResolutionSet FromConstraintSet(
      const blink::WebMediaTrackConstraintSet& constraint_set);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamConstraintsUtilSetsTest,
                           ResolutionVertices);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamConstraintsUtilSetsTest,
                           ResolutionPointSetClosestPoint);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamConstraintsUtilSetsTest,
                           ResolutionLineSetClosestPoint);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamConstraintsUtilSetsTest,
                           ResolutionGeneralSetClosestPoint);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamConstraintsUtilSetsTest,
                           ResolutionIdealOutsideSinglePoint);

  // Implements SelectClosestPointToIdeal() for the case when only the ideal
  // aspect ratio is provided.
  Point SelectClosestPointToIdealAspectRatio(double ideal_aspect_ratio,
                                             int default_height,
                                             int default_width) const;

  // Returns the closest point in this set to |point|. If |point| is included in
  // this set, Point is returned. If this set is empty, behavior is undefined.
  Point ClosestPointTo(const Point& point) const;

  // Returns the vertices of the set that have the property accessed
  // by |accessor| closest to |value|. The returned vector always has one or two
  // elements. Behavior is undefined if the set is empty.
  std::vector<Point> GetClosestVertices(double (Point::*accessor)() const,
                                        double value) const;

  // Returns a list of the vertices defined by the constraints on a height-width
  // Cartesian plane.
  // If the list is empty, the set is empty.
  // If the list contains a single point, the set contains a single point.
  // If the list contains two points, the set is composed of points on a line
  // segment.
  // If the list contains three to six points, they are the vertices of a
  // convex polygon containing all valid points in the set. Each pair of
  // consecutive vertices (modulo the size of the list) corresponds to a side of
  // the polygon, with the vertices given in counterclockwise order.
  // The list cannot contain more than six points.
  std::vector<Point> ComputeVertices() const;

  // Adds |point| to |vertices| if |point| is included in this candidate set.
  void TryAddVertex(std::vector<ResolutionSet::Point>* vertices,
                    const ResolutionSet::Point& point) const;

  int min_height_;
  int max_height_;
  int min_width_;
  int max_width_;
  double min_aspect_ratio_;
  double max_aspect_ratio_;
};

// Scalar multiplication for Points.
ResolutionSet::Point CONTENT_EXPORT operator*(double d,
                                              const ResolutionSet::Point& p);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_SETS_H_
