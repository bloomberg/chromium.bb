// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_COMMON_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/wm/wm_snap_to_pixel_layout_manager.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace gfx {
class Rect;
}

namespace ash {
class WindowDimmer;

// LayoutManager for the modal window container.
// System modal windows which are centered on the screen will be kept centered
// when the container size changes.
class ASH_EXPORT SystemModalContainerLayoutManager
    : public wm::WmSnapToPixelLayoutManager,
      public aura::WindowObserver,
      public keyboard::KeyboardControllerObserver {
 public:
  explicit SystemModalContainerLayoutManager(WmWindow* container);
  ~SystemModalContainerLayoutManager() override;

  bool has_window_dimmer() const { return window_dimmer_ != nullptr; }

  // Overridden from WmSnapToPixelLayoutManager:
  void OnChildWindowVisibilityChanged(WmWindow* child, bool visible) override;
  void OnWindowResized() override;
  void OnWindowAddedToLayout(WmWindow* child) override;
  void OnWillRemoveWindowFromLayout(WmWindow* child) override;
  void SetChildBounds(WmWindow* child,
                      const gfx::Rect& requested_bounds) override;

  // Overridden from aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // Overridden from keyboard::KeyboardControllerObserver:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;
  void OnKeyboardClosed() override;

  // True if the window is either contained by the top most modal window,
  // or contained by its transient children.
  bool IsPartOfActiveModalWindow(WmWindow* window);

  // Activates next modal window if any. Returns false if there
  // are no more modal windows in this layout manager.
  bool ActivateNextModalWindow();

  // Creates modal background window, which is a partially-opaque
  // fullscreen window. If there is already a modal background window,
  // it will bring it the top.
  void CreateModalBackground();

  void DestroyModalBackground();

  // Is the |window| modal background?
  static bool IsModalBackground(WmWindow* window);

 private:
  void AddModalWindow(WmWindow* window);

  // Removes |window| from |modal_windows_|. Returns true if |window| was in
  // |modal_windows_|.
  bool RemoveModalWindow(WmWindow* window);

  // Reposition the dialogs to become visible after the work area changes.
  void PositionDialogsAfterWorkAreaResize();

  // Get the usable bounds rectangle for enclosed dialogs.
  gfx::Rect GetUsableDialogArea() const;

  // Gets the new bounds for a |window| to use which are either centered (if the
  // window was previously centered) or fitted to the screen.
  gfx::Rect GetCenteredAndOrFittedBounds(const WmWindow* window);

  // Returns true if |bounds| is considered centered.
  bool IsBoundsCentered(const gfx::Rect& window_bounds) const;

  WmWindow* modal_window() {
    return !modal_windows_.empty() ? modal_windows_.back() : nullptr;
  }

  // The container that owns the layout manager.
  WmWindow* container_;

  // WindowDimmer used to dim windows behind the modal window(s) being shown in
  // |container_|.
  std::unique_ptr<WindowDimmer> window_dimmer_;

  // A stack of modal windows. Only the topmost can receive events.
  std::vector<WmWindow*> modal_windows_;

  // Windows contained in this set are centered. Windows are automatically
  // added to this based on IsBoundsCentered().
  std::set<const WmWindow*> windows_to_center_;

  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerLayoutManager);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
