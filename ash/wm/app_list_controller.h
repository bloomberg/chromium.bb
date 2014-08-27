// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_APP_LIST_CONTROLLER_H_
#define ASH_WM_APP_LIST_CONTROLLER_H_

#include "ash/shelf/shelf_icon_observer.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "ui/app_list/pagination_model_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace app_list {
class ApplicationDragAndDropHost;
class AppListView;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
namespace test {
class AppListControllerTestApi;
}

// AppListController is a controller that manages app list UI for shell.
// It creates AppListView and schedules showing/hiding animation.
// While the UI is visible, it monitors things such as app list widget's
// activation state and desktop mouse click to auto dismiss the UI.
class AppListController : public ui::EventHandler,
                          public aura::client::FocusChangeObserver,
                          public aura::WindowObserver,
                          public ui::ImplicitAnimationObserver,
                          public views::WidgetObserver,
                          public keyboard::KeyboardControllerObserver,
                          public ShellObserver,
                          public ShelfIconObserver,
                          public app_list::PaginationModelObserver {
 public:
  AppListController();
  virtual ~AppListController();

  // Show/hide app list window. The |window| is used to deterime in
  // which display (in which the |window| exists) the app list should
  // be shown.
  void Show(aura::Window* window);
  void Dismiss();

  // Whether app list window is visible (shown or being shown).
  bool IsVisible() const;

  // Returns target visibility. This differs from IsVisible() if an animation
  // is ongoing.
  bool GetTargetVisibility() const { return is_visible_; }

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetWindow();

  // Returns app list view or NULL if it is not visible.
  app_list::AppListView* GetView() { return view_; }

 private:
  friend class test::AppListControllerTestApi;

  // If |drag_and_drop_host| is not NULL it will be called upon drag and drop
  // operations outside the application list.
  void SetDragAndDropHostOfCurrentAppList(
      app_list::ApplicationDragAndDropHost* drag_and_drop_host);

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

  // aura::WindowObserver overrides:
  virtual void OnWindowBoundsChanged(aura::Window* root,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // views::WidgetObserver overrides:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // KeyboardControllerObserver overrides:
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnShelfAlignmentChanged(aura::Window* root_window) OVERRIDE;

  // ShelfIconObserver overrides:
  virtual void OnShelfIconPositionsChanged() OVERRIDE;

  // app_list::PaginationModelObserver overrides:
  virtual void TotalPagesChanged() OVERRIDE;
  virtual void SelectedPageChanged(int old_selected, int new_selected) OVERRIDE;
  virtual void TransitionStarted() OVERRIDE;
  virtual void TransitionChanged() OVERRIDE;

  // Whether we should show or hide app list widget.
  bool is_visible_;

  // Whether the app list should remain centered.
  bool is_centered_;

  // The AppListView this class manages, owned by its widget.
  app_list::AppListView* view_;

  // The current page of the AppsGridView of |view_|. This is stored outside of
  // the view's PaginationModel, so that it persists when the view is destroyed.
  int current_apps_page_;

  // Cached bounds of |view_| for snapping back animation after over-scroll.
  gfx::Rect view_bounds_;

  // Whether should schedule snap back animation.
  bool should_snap_back_;

  DISALLOW_COPY_AND_ASSIGN(AppListController);
};

}  // namespace ash

#endif  // ASH_WM_APP_LIST_CONTROLLER_H_
