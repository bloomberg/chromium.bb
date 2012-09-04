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
#include "ui/aura/event_filter.h"
#include "ui/aura/focus_change_observer.h"
#include "ui/aura/root_window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
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
class AppListController : public aura::EventFilter,
                          public aura::FocusChangeObserver,
                          public aura::RootWindowObserver,
                          public ui::ImplicitAnimationObserver,
                          public views::WidgetObserver,
                          public ShellObserver,
                          public LauncherIconObserver {
 public:
  AppListController();
  virtual ~AppListController();

  // Show/hide app list window.
  void SetVisible(bool visible);

  // Whether app list window is visible (shown or being shown).
  bool IsVisible() const;

  // Returns target visibility. This differs from IsVisible() if an animation
  // is ongoing.
  bool GetTargetVisibility() const { return is_visible_; }

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetWindow();

 private:
  // Sets app list view. If we are in visible mode, start showing animation.
  // Otherwise, we just close it.
  void SetView(app_list::AppListView* view);

  // Forgets the view.
  void ResetView();

  // Starts show/hide animation.
  void ScheduleAnimation();

  void ProcessLocatedEvent(aura::Window* target,
                           const ui::LocatedEvent& event);

  // Makes app list bubble update its bounds.
  void UpdateBounds();

  // aura::EventFilter overrides:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

  // aura::FocusChangeObserver overrides:
  virtual void OnWindowFocused(aura::Window* window) OVERRIDE;

  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // views::WidgetObserver overrides:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnShelfAlignmentChanged() OVERRIDE;

  // LauncherIconObserver overrides:
  virtual void OnLauncherIconPositionsChanged() OVERRIDE;

  scoped_ptr<app_list::PaginationModel> pagination_model_;

  // Whether we should show or hide app list widget.
  bool is_visible_;

  // The AppListView this class manages, owned by its widget.
  app_list::AppListView* view_;

  DISALLOW_COPY_AND_ASSIGN(AppListController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_APP_LIST_CONTROLLER_H_
