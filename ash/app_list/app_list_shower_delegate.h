// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_SHOWER_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_SHOWER_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_icon_observer.h"
#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/app_list/shower/app_list_shower_delegate.h"
#include "ui/events/event_handler.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace app_list {
class ApplicationDragAndDropHost;
class AppListView;
}

namespace app_list {
class AppListShower;
}

namespace ui {
class LocatedEvent;
}

namespace ash {

namespace test {
class AppListShowerAshTestApi;
}

class AppListViewDelegateFactory;

// Responsible for laying out the app list UI as well as updating the Shelf
// launch icon as the state of the app list changes. Listens to shell events
// and touches/mouse clicks outside the app list to auto dismiss the UI or
// update its layout as necessary.
class ASH_EXPORT AppListShowerDelegate
    : public app_list::AppListShowerDelegate,
      public ui::EventHandler,
      public keyboard::KeyboardControllerObserver,
      public ShellObserver,
      public ShelfIconObserver {
 public:
  AppListShowerDelegate(app_list::AppListShower* shower,
                        AppListViewDelegateFactory* view_delegate_factory);
  ~AppListShowerDelegate() override;

  // app_list::AppListShowerDelegate:
  app_list::AppListViewDelegate* GetViewDelegate() override;
  void Init(app_list::AppListView* view,
            aura::Window* root_window,
            int current_apps_page) override;
  void OnShown(aura::Window* root_window) override;
  void OnDismissed() override;
  void UpdateBounds() override;
  gfx::Vector2d GetVisibilityAnimationOffset(
      aura::Window* root_window) override;

 private:
  void ProcessLocatedEvent(ui::LocatedEvent* event);

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // KeyboardControllerObserver overrides:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;

  // ShellObserver overrides:
  void OnShelfAlignmentChanged(aura::Window* root_window) override;
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

  // ShelfIconObserver overrides:
  void OnShelfIconPositionsChanged() override;

  // Whether the app list is visible (or in the process of being shown).
  bool is_visible_ = false;

  // Whether the app list should remain centered.
  bool is_centered_ = false;

  // Not owned. Pointer is guaranteed to be valid while this object is alive.
  app_list::AppListShower* shower_;

  // Not owned. Pointer is guaranteed to be valid while this object is alive.
  AppListViewDelegateFactory* view_delegate_factory_;

  // The AppListView this class manages, owned by its widget.
  app_list::AppListView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppListShowerDelegate);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_SHOWER_DELEGATE_H_
