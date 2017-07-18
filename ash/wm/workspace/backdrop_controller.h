// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_BACKDROP_DELEGATE_IMPL_H_
#define ASH_WM_WORKSPACE_WORKSPACE_BACKDROP_DELEGATE_IMPL_H_

#include <memory>

#include "ash/shell_observer.h"
#include "ash/system/accessibility_observer.h"
#include "ash/wm/wm_types.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ui {
class EventHandler;
}

namespace ash {

namespace wm {
class WindowState;
}

class BackdropDelegate;

// A backdrop which gets created for a container |window| and which gets
// stacked behind the top level, activatable window that meets the following
// criteria.
//
// 1) Has a aura::client::kHasBackdrop property = true.
// 2) BackdropDelegate::HasBackdrop(aura::Window* window) returns true.
// 3) Active ARC window when the spoken feedback is enabled.
class BackdropController : public ShellObserver, public AccessibilityObserver {
 public:
  explicit BackdropController(aura::Window* container);
  ~BackdropController() override;

  void OnWindowAddedToLayout(aura::Window* child);
  void OnWindowRemovedFromLayout(aura::Window* child);
  void OnChildWindowVisibilityChanged(aura::Window* child, bool visible);
  void OnWindowStackingChanged(aura::Window* window);
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type);

  void SetBackdropDelegate(std::unique_ptr<BackdropDelegate> delegate);

  // Update the visibility of, and restack the backdrop relative to
  // the other windows in the container.
  void UpdateBackdrop();

  // ShellObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;
  void OnSplitViewModeStarting() override;
  void OnSplitViewModeEnded() override;

  // AccessibilityObserver:
  void OnAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify) override;

 private:
  friend class WorkspaceControllerTestApi;

  void EnsureBackdropWidget();

  void UpdateAccessibilityMode();

  // Returns the current visible top level window in the container.
  aura::Window* GetTopmostWindowWithBackdrop();

  bool WindowShouldHaveBackdrop(aura::Window* window);

  // Show the backdrop window.
  void Show();

  // Hide the backdrop window.
  void Hide();

  // The backdrop which covers the rest of the screen.
  views::Widget* backdrop_ = nullptr;

  // aura::Window for |backdrop_|.
  aura::Window* backdrop_window_ = nullptr;

  // The container of the window that should have a backdrop.
  aura::Window* container_;

  std::unique_ptr<BackdropDelegate> delegate_;

  // Event hanlder used to implement actions for accessibility.
  std::unique_ptr<ui::EventHandler> backdrop_event_handler_;
  ui::EventHandler* original_event_handler_ = nullptr;

  // If true, the |RestackOrHideWindow| might recurse.
  bool in_restacking_ = false;

  // True to temporarily hide the backdrop. Used in
  // overview mode.
  bool force_hidden_ = false;

  DISALLOW_COPY_AND_ASSIGN(BackdropController);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_BACKDROP_DELEGATE_IMPL_H_
