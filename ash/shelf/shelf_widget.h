// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WIDGET_H_
#define ASH_SHELF_SHELF_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/shelf/background_animator.h"
#include "ash/shelf/shelf_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}

namespace ash {
class Launcher;

namespace internal {
class FocusCycler;
class StatusAreaWidget;
class ShelfLayoutManager;
class WorkspaceController;
}

class ASH_EXPORT ShelfWidget : public views::Widget,
                               public views::WidgetObserver {
 public:
  ShelfWidget(
      aura::Window* shelf_container,
      aura::Window* status_container,
      internal::WorkspaceController* workspace_controller);
  virtual ~ShelfWidget();

  void SetAlignment(ShelfAlignment alignmnet);
  ShelfAlignment GetAlignment() const;

  // Sets whether the shelf paints a background. Default is false, but is set
  // to true if a window overlaps the shelf.
  void SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type);
  bool paints_background() const {
    return background_animator_.paints_background();
  }

  // Causes shelf items to be slightly dimmed (eg when a window is maximized).
  void SetDimsShelf(bool dimming);
  bool GetDimsShelf() const;

  internal::ShelfLayoutManager* shelf_layout_manager() {
    return shelf_layout_manager_;
  }
  Launcher* launcher() const { return launcher_.get(); }
  internal::StatusAreaWidget* status_area_widget() const {
    return status_area_widget_;
  }

  void CreateLauncher();

  // Set visibility of the launcher component of the shelf.
  void SetLauncherVisibility(bool visible);
  bool IsLauncherVisible() const;

  // Sets the focus cycler.  Also adds the launcher to the cycle.
  void SetFocusCycler(internal::FocusCycler* focus_cycler);
  internal::FocusCycler* GetFocusCycler();

  // Called by the activation delegate, before the launcher is activated
  // when no other windows are visible.
  void WillActivateAsFallback() { activating_as_fallback_ = true; }

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetActivationChanged(
      views::Widget* widget, bool active) OVERRIDE;

  aura::Window* window_container() { return window_container_; }

  // TODO(harrym): Remove when Status Area Widget is a child view.
  void ShutdownStatusAreaWidget();

 private:
  class DelegateView;

  internal::ShelfLayoutManager* shelf_layout_manager_;
  scoped_ptr<Launcher> launcher_;
  internal::StatusAreaWidget* status_area_widget_;

  // delegate_view_ is attached to window_container_ and is cleaned up
  // during CloseChildWindows of the associated RootWindowController.
  DelegateView* delegate_view_;
  internal::BackgroundAnimator background_animator_;
  bool activating_as_fallback_;
  aura::Window* window_container_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WIDGET_H_
