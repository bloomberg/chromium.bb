// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SNAP_SELECTION_STRATEGY_H_
#define CC_INPUT_SNAP_SELECTION_STRATEGY_H_

#include "scroll_snap_data.h"

#include <memory>

namespace cc {

enum class SnapStopAlwaysFilter { kIgnore, kRequire };
enum class SnapTargetsPrioritization { kIgnore, kRequire };

// This class represents an abstract strategy that decide which snap selection
// should be considered valid. There are concrete implementations for three core
// scrolling types: scroll with end position only, scroll with direction only,
// and scroll with end position and direction.
class CC_EXPORT SnapSelectionStrategy {
 public:
  SnapSelectionStrategy() = default;
  virtual ~SnapSelectionStrategy() = default;
  static std::unique_ptr<SnapSelectionStrategy> CreateForEndPosition(
      const gfx::ScrollOffset& current_position,
      bool scrolled_x,
      bool scrolled_y,
      SnapTargetsPrioritization prioritization =
          SnapTargetsPrioritization::kIgnore);

  // |use_fractional_offsets| should be true when the current position is
  // provided in fractional pixels.
  static std::unique_ptr<SnapSelectionStrategy> CreateForDirection(
      gfx::ScrollOffset current_position,
      gfx::ScrollOffset step,
      bool use_fractional_offsets,
      SnapStopAlwaysFilter filter = SnapStopAlwaysFilter::kIgnore);
  static std::unique_ptr<SnapSelectionStrategy> CreateForEndAndDirection(
      gfx::ScrollOffset current_position,
      gfx::ScrollOffset displacement,
      bool use_fractional_offsets);

  // Creates a selection strategy that attempts to snap to previously snapped
  // targets if possible, but defaults to finding the closest snap point if
  // the target no longer exists.
  static std::unique_ptr<SnapSelectionStrategy> CreateForTargetElement(
      gfx::ScrollOffset current_position);

  // Returns whether it's snappable on x or y depending on the scroll performed.
  virtual bool ShouldSnapOnX() const = 0;
  virtual bool ShouldSnapOnY() const = 0;

  // Returns whether snapping should attempt to snap to the previously snapped
  // area if possible.
  virtual bool ShouldPrioritizeSnapTargets() const;

  // Returns the end position of the scroll if no snap interferes.
  virtual gfx::ScrollOffset intended_position() const = 0;
  // Returns the scroll position from which the snap position should minimize
  // its distance.
  virtual gfx::ScrollOffset base_position() const = 0;
  // Returns the current scroll position of the snap container.
  const gfx::ScrollOffset& current_position() const {
    return current_position_;
  }

  // Returns true if the selection strategy considers the given snap offset
  // valid for the current axis.
  virtual bool IsValidSnapPosition(SearchAxis axis, float position) const = 0;
  virtual bool IsValidSnapArea(SearchAxis axis, const SnapAreaData& data) const;

  virtual bool HasIntendedDirection() const;

  // Returns true if a snap area with scroll-snap-stop:always should not be
  // bypassed.
  virtual bool ShouldRespectSnapStop() const;

  // Returns the best result according to snap selection strategy. This method
  // is called at the end of selection process to make the final decision.
  //
  // -closest: snap search result representing closest match.
  // -covering: snap search result representing the original target if it makes
  //            a snaparea covering the snapport.
  virtual const base::Optional<SnapSearchResult>& PickBestResult(
      const base::Optional<SnapSearchResult>& closest,
      const base::Optional<SnapSearchResult>& covering) const = 0;

  // Returns true when the current scroll offset is provided in fractional
  // pixels.
  virtual bool UsingFractionalOffsets() const;

 protected:
  explicit SnapSelectionStrategy(const gfx::ScrollOffset& current_position)
      : current_position_(current_position) {}
  const gfx::ScrollOffset current_position_;
};

// Examples for intended end position scrolls include
// - a panning gesture, released without momentum
// - manupulating the scrollbar "thumb" explicitly
// - programmatically scrolling via APIs such as scrollTo()
// - tabbing through the document's focusable elements
// - navigating to an anchor within the page
// - homing operations such as the Home/End keys
// For this type of scrolls, we want to
// * Minimize the distance between the snap position and the end position.
// * Return the end position if that makes a snap area covers the snapport.
class EndPositionStrategy : public SnapSelectionStrategy {
 public:
  EndPositionStrategy(const gfx::ScrollOffset& current_position,
                      bool scrolled_x,
                      bool scrolled_y,
                      SnapTargetsPrioritization snap_targets_prioritization)
      : SnapSelectionStrategy(current_position),
        scrolled_x_(scrolled_x),
        scrolled_y_(scrolled_y),
        snap_targets_prioritization_(snap_targets_prioritization) {}
  ~EndPositionStrategy() override = default;

  bool ShouldSnapOnX() const override;
  bool ShouldSnapOnY() const override;

  gfx::ScrollOffset intended_position() const override;
  gfx::ScrollOffset base_position() const override;

  bool IsValidSnapPosition(SearchAxis axis, float position) const override;
  bool HasIntendedDirection() const override;
  bool ShouldPrioritizeSnapTargets() const override;

  const base::Optional<SnapSearchResult>& PickBestResult(
      const base::Optional<SnapSearchResult>& closest,
      const base::Optional<SnapSearchResult>& covering) const override;

 private:
  // Whether the x axis and y axis have been scrolled in this scroll gesture.
  const bool scrolled_x_;
  const bool scrolled_y_;
  SnapTargetsPrioritization snap_targets_prioritization_;
};

// Examples for intended direction scrolls include
// - pressing an arrow key on the keyboard
// - a swiping gesture interpreted as a fixed (rather than inertial) scroll
// For this type of scrolls, we want to
// * Minimize the distance between the snap position and the starting position,
//   so that we stop at the first snap position in that direction.
// * Return the default intended position (using the default step) if that makes
//   a snap area covers the snapport.
class DirectionStrategy : public SnapSelectionStrategy {
 public:
  // |use_fractional_offsets| should be true when the current position is
  // provided in fractional pixels.
  DirectionStrategy(const gfx::ScrollOffset& current_position,
                    const gfx::ScrollOffset& step,
                    SnapStopAlwaysFilter filter,
                    bool use_fractional_offsets)
      : SnapSelectionStrategy(current_position),
        step_(step),
        snap_stop_always_filter_(filter),
        use_fractional_offsets_(use_fractional_offsets) {}
  ~DirectionStrategy() override = default;

  bool ShouldSnapOnX() const override;
  bool ShouldSnapOnY() const override;

  gfx::ScrollOffset intended_position() const override;
  gfx::ScrollOffset base_position() const override;

  bool IsValidSnapPosition(SearchAxis axis, float position) const override;
  bool IsValidSnapArea(SearchAxis axis,
                       const SnapAreaData& area) const override;

  const base::Optional<SnapSearchResult>& PickBestResult(
      const base::Optional<SnapSearchResult>& closest,
      const base::Optional<SnapSearchResult>& covering) const override;

  bool UsingFractionalOffsets() const override;

 private:
  // The default step for this DirectionStrategy.
  const gfx::ScrollOffset step_;
  SnapStopAlwaysFilter snap_stop_always_filter_;
  bool use_fractional_offsets_;
};

// Examples for intended direction and end position scrolls include
// - a “fling” gesture, interpreted with momentum
// - programmatically scrolling via APIs such as scrollBy()
// - paging operations such as the PgUp/PgDn keys (or equivalent operations on
//   the scrollbar)
// For this type of scrolls, we want to
// * Minimize the distance between the snap position and the end position.
// * Return the end position if that makes a snap area covers the snapport.
class EndAndDirectionStrategy : public SnapSelectionStrategy {
 public:
  // |use_fractional_offsets| should be true when the current position is
  // provided in fractional pixels.
  EndAndDirectionStrategy(const gfx::ScrollOffset& current_position,
                          const gfx::ScrollOffset& displacement,
                          bool use_fractional_offsets)
      : SnapSelectionStrategy(current_position),
        displacement_(displacement),
        use_fractional_offsets_(use_fractional_offsets) {}
  ~EndAndDirectionStrategy() override = default;

  bool ShouldSnapOnX() const override;
  bool ShouldSnapOnY() const override;

  gfx::ScrollOffset intended_position() const override;
  gfx::ScrollOffset base_position() const override;

  bool IsValidSnapPosition(SearchAxis axis, float position) const override;

  bool ShouldRespectSnapStop() const override;

  const base::Optional<SnapSearchResult>& PickBestResult(
      const base::Optional<SnapSearchResult>& closest,
      const base::Optional<SnapSearchResult>& covering) const override;

  bool UsingFractionalOffsets() const override;

 private:
  const gfx::ScrollOffset displacement_;
  bool use_fractional_offsets_;
};

}  // namespace cc

#endif  // CC_INPUT_SNAP_SELECTION_STRATEGY_H_
