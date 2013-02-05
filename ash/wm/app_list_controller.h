// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_APP_LIST_CONTROLLER_H_
#define ASH_WM_APP_LIST_CONTROLLER_H_

#include "ash/launcher/launcher_icon_observer.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "ui/app_list/pagination_model_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/root_window_observer.h"
#include "ui/base/events/event_handler.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget_observer.h"

namespace app_list {
class AppListView;
class PaginationModel;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
namespace internal {

// AppListController is a controller that manages app list UI for shell.
// It creates AppListView and schedules showing/hiding animation.
// While the UI is visible, it monitors things such as app list widget's
// activation state and desktop mouse click to auto dismiss the UI.
class AppListController : public ui::EventHandler,
                          public aura::client::FocusChangeObserver,
                          public aura::RootWindowObserver,
                          public ui::ImplicitAnimationObserver,
                          public views::WidgetObserver,
                          public ShellObserver,
                          public LauncherIconObserver,
                          public app_list::PaginationModelObserver {
 public:
  AppListController();
  virtual ~AppListController();

  // Show/hide app list window. The |window| is used to deterime in
  // which display (in which the |window| exists) the app list should
  // be shown.
  void SetVisible(bool visible, aura::Window* window);

  // Whether app list window is visible (shown or being shown).
  bool IsVisible() const;

  // Returns target visibility. This differs from IsVisible() if an animation
  // is ongoing.
  bool GetTargetVisibility() const { return is_visible_; }

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetWindow();

 private:
  // Sets the app list view and attempts to show it.
  void SetView(app_list::AppListView* view);

  // Forgets the view.
  void ResetView();

  // Starts show/hide animation.
  void ScheduleAnimation();

  void ProcessLocatedEvent(ui::LocatedEvent* event);

  // Makes app list bubble update its bounds.
  void UpdateBounds();

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // aura::client::FocusChangeObserver overrides:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) OVERRIDE;

  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // views::WidgetObserver overrides:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnShelfAlignmentChanged(aura::RootWindow* root_window) OVERRIDE;

  // LauncherIconObserver overrides:
  virtual void OnLauncherIconPositionsChanged() OVERRIDE;

  // app_list::PaginationModelObserver overrides:
  virtual void TotalPagesChanged() OVERRIDE;
  virtual void SelectedPageChanged(int old_selected, int new_selected) OVERRIDE;
  virtual void TransitionChanged() OVERRIDE;

  scoped_ptr<app_list::PaginationModel> pagination_model_;

  // Whether we should show or hide app list widget.
  bool is_visible_;

  // The AppListView this class manages, owned by its widget.
  app_list::AppListView* view_;

  // Cached bounds of |view_| for snapping back animation after over-scroll.
  gfx::Rect view_bounds_;

  // Whether should schedule snap back animation.
  bool should_snap_back_;

  DISALLOW_COPY_AND_ASSIGN(AppListController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_APP_LIST_CONTROLLER_H_
