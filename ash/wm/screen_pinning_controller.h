// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCREEN_PINNING_CONTROLLER_H_
#define ASH_WM_SCREEN_PINNING_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"

namespace ash {

class WindowTreeHostManager;
class WmWindow;

// Handles pinned state.
class ScreenPinningController : public WindowTreeHostManager::Observer {
 public:
  explicit ScreenPinningController(
      WindowTreeHostManager* window_tree_host_manager);
  ~ScreenPinningController() override;

  // Sets a pinned window. It is not allowed to call this when there already
  // is a pinned window.
  void SetPinnedWindow(WmWindow* pinned_window);

  // Returns true if in pinned mode, otherwise false.
  bool IsPinned() const;

  // Returns the pinned window if in pinned mode, or nullptr.
  WmWindow* pinned_window() const { return pinned_window_; }

  // Called when a new window is added to the container which has the pinned
  // window.
  void OnWindowAddedToPinnedContainer(WmWindow* new_window);

  // Called when a window will be removed from the container which has the
  // pinned window.
  void OnWillRemoveWindowFromPinnedContainer(WmWindow* window);

  // Called when a window stacking is changed in the container which has the
  // pinned window.
  void OnPinnedContainerWindowStackingChanged(WmWindow* window);

  // Called when a new window is added to a system modal container.
  void OnWindowAddedToSystemModalContainer(WmWindow* new_window);

  // Called when a window will be removed from a system modal container.
  void OnWillRemoveWindowFromSystemModalContainer(WmWindow* window);

  // Called when a window stacking is changed in a system modal container.
  void OnSystemModalContainerWindowStackingChanged(WmWindow* window);

  // Called when a dim window in the system modal container is destroying.
  void OnDimWindowDestroying(WmWindow* window);

 private:
  class PinnedContainerWindowObserver;
  class PinnedContainerChildWindowObserver;
  class SystemModalContainerWindowObserver;
  class SystemModalContainerChildWindowObserver;
  class DimWindowObserver;

  // Keeps the pinned window on top of the siblings.
  void KeepPinnedWindowOnTop();

  // Keeps the dim window at bottom of the container.
  void KeepDimWindowAtBottom(WmWindow* container);

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // Pinned window should be on top in the parent window.
  WmWindow* pinned_window_ = nullptr;

  // Dim background window just behind of the pinned window.
  // Not owned. The parent has its ownership.
  WmWindow* background_window_ = nullptr;

  // In pinned mode, all displays other than the one where pinned_window_ is.
  // Similar to background_window_, not owned.
  std::vector<WmWindow*> dim_windows_;

  // Set true only when restacking done by this controller.
  bool in_restacking_ = false;

  // Keep references to remove this as a observer.
  // While this controller is alive, it needs to be ensured that the instances
  // refered from the pointers should be alive.
  WindowTreeHostManager* window_tree_host_manager_;

  // Window observers to translate events for the window to this controller.
  std::unique_ptr<PinnedContainerWindowObserver>
      pinned_container_window_observer_;
  std::unique_ptr<PinnedContainerChildWindowObserver>
      pinned_container_child_window_observer_;
  std::unique_ptr<SystemModalContainerWindowObserver>
      system_modal_container_window_observer_;
  std::unique_ptr<SystemModalContainerChildWindowObserver>
      system_modal_container_child_window_observer_;
  std::unique_ptr<DimWindowObserver> dim_window_observer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenPinningController);
};

}  // namespace ash

#endif  // ASH_WM_SCREEN_PINNING_CONTROLLER_H_
