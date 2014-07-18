// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/snap_to_pixel_layout_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace aura {
class Window;
class EventFilter;
}
namespace gfx {
class Rect;
}
namespace views {
class Widget;
}

namespace ash {

// LayoutManager for the modal window container.
// System modal windows which are centered on the screen will be kept centered
// when the container size changes.
class ASH_EXPORT SystemModalContainerLayoutManager
    : public SnapToPixelLayoutManager,
      public aura::WindowObserver,
      public keyboard::KeyboardControllerObserver {
 public:
  explicit SystemModalContainerLayoutManager(aura::Window* container);
  virtual ~SystemModalContainerLayoutManager();

  bool has_modal_background() const { return modal_background_ != NULL; }

  // Overridden from SnapToPixelLayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overridden from keyboard::KeyboardControllerObserver:
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) OVERRIDE;

  // Can a given |window| receive and handle input events?
  bool CanWindowReceiveEvents(aura::Window* window);

  // Activates next modal window if any. Returns false if there
  // are no more modal windows in this layout manager.
  bool ActivateNextModalWindow();

  // Creates modal background window, which is a partially-opaque
  // fullscreen window. If there is already a modal background window,
  // it will bring it the top.
  void CreateModalBackground();

  void DestroyModalBackground();

  // Is the |window| modal background?
  static bool IsModalBackground(aura::Window* window);

 private:
  void AddModalWindow(aura::Window* window);
  void RemoveModalWindow(aura::Window* window);

  // Reposition the dialogs to become visible after the work area changes.
  void PositionDialogsAfterWorkAreaResize();

  // Get the usable bounds rectangle for enclosed dialogs.
  gfx::Rect GetUsableDialogArea();

  // Gets the new bounds for a |window| to use which are either centered (if the
  // window was previously centered) or fitted to the screen.
  gfx::Rect GetCenteredAndOrFittedBounds(const aura::Window* window);

  // Returns true if |window_bounds| is centered.
  bool DialogIsCentered(const gfx::Rect& window_bounds);

  aura::Window* modal_window() {
    return !modal_windows_.empty() ? modal_windows_.back() : NULL;
  }

  // The container that owns the layout manager.
  aura::Window* container_;

  // A widget that dims the windows behind the modal window(s) being
  // shown in |container_|.
  views::Widget* modal_background_;

  // A stack of modal windows. Only the topmost can receive events.
  std::vector<aura::Window*> modal_windows_;

  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
