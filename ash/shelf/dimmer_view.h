// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DIMMER_VIEW_H_
#define ASH_SHELF_DIMMER_VIEW_H_

#include "ash/common/shelf/wm_dimmer_view.h"
#include "ash/common/wm/background_animator.h"
#include "ash/common/wm_window_observer.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class WmShelf;

// DimmerView slightly dims shelf items when a window is maximized and visible.
// TODO(jamescook): Delete this after material design ships, as MD will not
// require shelf dimming. http://crbug.com/614453
class DimmerView : public views::View,
                   public views::WidgetDelegate,
                   public BackgroundAnimatorDelegate,
                   public WmDimmerView,
                   public WmWindowObserver {
 public:
  // Creates and shows a DimmerView and its Widget. The returned view is owned
  // by its widget. If |disable_animations_for_test| is set, all changes apply
  // instantly.
  static DimmerView* Create(WmShelf* shelf, bool disable_animations_for_test);

  // Called by |DimmerEventFilter| when the mouse |hovered| state changes.
  void SetHovered(bool hovered);

  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // views::WidgetDelegate overrides:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // BackgroundAnimatorDelegate overrides:
  void UpdateBackground(BackgroundAnimator* animator, int alpha) override;
  void BackgroundAnimationEnded(BackgroundAnimator* animator) override;

  // WmDimmerView:
  views::Widget* GetDimmerWidget() override;
  void ForceUndimming(bool force) override;
  int GetDimmingAlphaForTest() override;

  // WmWindowObserver overrides:
  // This will be called when the shelf itself changes its absolute position.
  // Since the |dimmer_| panel needs to be placed in screen coordinates it needs
  // to be repositioned. The difference to the OnBoundsChanged call above is
  // that this gets also triggered when the shelf only moves.
  void OnWindowBoundsChanged(WmWindow* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

 private:
  DimmerView(WmShelf* shelf, bool disable_animations_for_test);
  ~DimmerView() override;

  // This class monitors mouse events to see if it is on top of the shelf.
  class DimmerEventFilter : public ui::EventHandler {
   public:
    explicit DimmerEventFilter(DimmerView* owner);
    ~DimmerEventFilter() override;

    // Overridden from ui::EventHandler:
    void OnMouseEvent(ui::MouseEvent* event) override;
    void OnTouchEvent(ui::TouchEvent* event) override;

   private:
    // The owning class.
    DimmerView* owner_;

    // True if the mouse is inside the shelf.
    bool mouse_inside_;

    // True if a touch event is inside the shelf.
    bool touch_inside_;

    DISALLOW_COPY_AND_ASSIGN(DimmerEventFilter);
  };

  // The shelf that uses this dimmer.
  WmShelf* shelf_;

  // The alpha to use for covering the shelf.
  int alpha_;

  // True if the event filter claims that we should not be dimmed.
  bool is_hovered_;

  // True if someone forces us not to be dimmed (e.g. a menu is open).
  bool force_hovered_;

  // True if animations should be suppressed for a test.
  bool disable_animations_for_test_;

  // The animator for the background transitions.
  BackgroundAnimator background_animator_;

  // Notification of entering / exiting of the shelf area by mouse.
  std::unique_ptr<DimmerEventFilter> event_filter_;

  DISALLOW_COPY_AND_ASSIGN(DimmerView);
};

}  // namespace ash

#endif  // ASH_SHELF_DIMMER_VIEW_H_
