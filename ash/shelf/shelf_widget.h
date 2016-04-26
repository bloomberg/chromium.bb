// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WIDGET_H_
#define ASH_SHELF_SHELF_WIDGET_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/background_animator.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}

namespace ash {
class FocusCycler;
class Shelf;
class ShelfLayoutManager;
class StatusAreaWidget;
class WorkspaceController;

class ASH_EXPORT ShelfWidget : public views::Widget,
                               public views::WidgetObserver,
                               public ShelfLayoutManagerObserver {
 public:
  ShelfWidget(aura::Window* shelf_container,
              aura::Window* status_container,
              WorkspaceController* workspace_controller);
  ~ShelfWidget() override;

  // Returns if shelf alignment option is enabled, and the user is able to
  // adjust the alignment (guest and supervised mode users cannot for example).
  static bool ShelfAlignmentAllowed();

  void OnShelfAlignmentChanged();
  wm::ShelfAlignment GetAlignment() const;

  // Sets the shelf's background type.
  void SetPaintsBackground(wm::ShelfBackgroundType background_type,
                           BackgroundAnimatorChangeType change_type);
  wm::ShelfBackgroundType GetBackgroundType() const;

  // Hide the shelf behind a black bar during e.g. a user transition when |hide|
  // is true. The |animation_time_ms| will be used as animation duration.
  void HideShelfBehindBlackBar(bool hide, int animation_time_ms);
  bool IsShelfHiddenBehindBlackBar() const;

  // Causes shelf items to be slightly dimmed (e.g. when a window is maximized).
  void SetDimsShelf(bool dimming);
  bool GetDimsShelf() const;

  ShelfLayoutManager* shelf_layout_manager() { return shelf_layout_manager_; }
  Shelf* shelf() const { return shelf_.get(); }
  StatusAreaWidget* status_area_widget() const { return status_area_widget_; }

  void CreateShelf();

  // Set visibility of the shelf.
  void SetShelfVisibility(bool visible);
  bool IsShelfVisible() const;

  // Sets the focus cycler.  Also adds the shelf to the cycle.
  void SetFocusCycler(FocusCycler* focus_cycler);
  FocusCycler* GetFocusCycler();

  // Called by the activation delegate, before the shelf is activated
  // when no other windows are visible.
  void WillActivateAsFallback() { activating_as_fallback_ = true; }

  aura::Window* window_container() { return window_container_; }

  // TODO(harrym): Remove when Status Area Widget is a child view.
  void ShutdownStatusAreaWidget();

  // Force the shelf to be presented in an undimmed state.
  void ForceUndimming(bool force);

  // Overridden from views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // A function to test the current alpha used by the dimming bar. If there is
  // no dimmer active, the function will return -1.
  int GetDimmingAlphaForTest();

  // A function to test the bounds of the dimming bar. Returns gfx::Rect() if
  // the dimmer is inactive.
  gfx::Rect GetDimmerBoundsForTest();

  // Disable dimming animations for running tests.
  void DisableDimmingAnimationsForTest();

  // ShelfLayoutManagerObserver overrides:
  void WillDeleteShelf() override;

 private:
  class DelegateView;

  ShelfLayoutManager* shelf_layout_manager_;
  std::unique_ptr<Shelf> shelf_;
  StatusAreaWidget* status_area_widget_;

  // delegate_view_ is attached to window_container_ and is cleaned up
  // during CloseChildWindows of the associated RootWindowController.
  DelegateView* delegate_view_;
  BackgroundAnimator background_animator_;
  bool activating_as_fallback_;
  aura::Window* window_container_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WIDGET_H_
