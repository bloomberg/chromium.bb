// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLIT_VIEW_SPLIT_VIEW_OVERVIEW_OVERLAY_H_
#define ASH_WM_SPLIT_VIEW_SPLIT_VIEW_OVERVIEW_OVERLAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Enum which contains the possible states SplitViewOverviewOverlay can be in.
enum class IndicatorState {
  kNone,
  kDragArea,
  kCannotSnap,
  kPhantomLeft,
  kPhantomRight
};

// Enum which contains the indicators that SplitViewOverviewOverlay can display.
// Converted to a bitmask to make testing easier.
enum class IndicatorType {
  kLeftHighlight = 1,
  kLeftText = 2,
  kRightHighlight = 4,
  kRightText = 8
};

// An overlay in overview mode which guides users while they are attempting to
// enter splitview. Displays text and highlights when dragging an overview
// window. Displays a highlight of where the window will end up when an overview
// window has entered a snap region.
class ASH_EXPORT SplitViewOverviewOverlay {
 public:
  SplitViewOverviewOverlay();
  ~SplitViewOverviewOverlay();

  // Sets visiblity. The correct indicators will become visible based on the
  // split view controllers state. If |event_location| is located on a different
  // root window than |widget_|, |widget_| will reparent.
  void SetIndicatorState(IndicatorState indicator_state,
                         const gfx::Point& event_location);
  IndicatorState current_indicator_state() const {
    return current_indicator_state_;
  }

  // Called by owner of this class when display bounds changes are observed, so
  // that this class can relayout accordingly.
  void OnDisplayBoundsChanged();

  bool GetIndicatorTypeVisibilityForTesting(IndicatorType type) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SplitViewWindowSelectorTest,
                           SplitViewOverviewOverlayWidgetReparenting);
  class RotatedImageLabelView;
  class SplitViewOverviewOverlayView;

  // The root content view of |widget_|.
  SplitViewOverviewOverlayView* overlay_view_ = nullptr;

  IndicatorState current_indicator_state_ = IndicatorState::kNone;

  // The SplitViewOverviewOverlay widget. It covers the entire root window
  // and displays regions and text indicating where users should drag windows
  // enter split view.
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewOverviewOverlay);
};

}  // namespace ash

#endif  // ASH_WM_SPLIT_VIEW_SPLIT_VIEW_OVERVIEW_OVERLAY_H_
