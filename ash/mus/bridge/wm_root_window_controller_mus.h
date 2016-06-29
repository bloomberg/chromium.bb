// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_ROOT_WINDOW_CONTROLLER_MUS_H_
#define ASH_MUS_BRIDGE_WM_ROOT_WINDOW_CONTROLLER_MUS_H_

#include "ash/common/wm_root_window_controller.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace display {
class Display;
}

namespace mus {
class Window;
}

namespace ash {
namespace mus {

class RootWindowController;
class WmShellMus;
class WmWindowMus;

// WmRootWindowController implementations for mus.
class WmRootWindowControllerMus : public WmRootWindowController {
 public:
  WmRootWindowControllerMus(WmShellMus* shell,
                            RootWindowController* root_window_controller);
  ~WmRootWindowControllerMus() override;

  static WmRootWindowControllerMus* Get(::mus::Window* window) {
    return const_cast<WmRootWindowControllerMus*>(
        Get(const_cast<const ::mus::Window*>(window)));
  }
  static const WmRootWindowControllerMus* Get(const ::mus::Window* window);

  RootWindowController* root_window_controller() {
    return root_window_controller_;
  }

  void NotifyFullscreenStateChange(bool is_fullscreen);

  // Screen conversion functions.
  gfx::Point ConvertPointToScreen(const WmWindowMus* source,
                                  const gfx::Point& point) const;
  gfx::Point ConvertPointFromScreen(const WmWindowMus* target,
                                    const gfx::Point& point) const;

  const display::Display& GetDisplay() const;

  // WmRootWindowController:
  bool HasShelf() override;
  WmShell* GetShell() override;
  wm::WorkspaceWindowState GetWorkspaceWindowState() override;
  void SetMaximizeBackdropDelegate(
      std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate)
      override;
  AlwaysOnTopController* GetAlwaysOnTopController() override;
  WmShelf* GetShelf() override;
  WmWindow* GetWindow() override;
  void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      int shell_container_id,
      views::Widget::InitParams* init_params) override;
  WmWindow* FindEventTarget(const gfx::Point& location_in_screen) override;
  void AddObserver(WmRootWindowControllerObserver* observer) override;
  void RemoveObserver(WmRootWindowControllerObserver* observer) override;

 private:
  WmShellMus* shell_;
  RootWindowController* root_window_controller_;
  base::ObserverList<WmRootWindowControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WmRootWindowControllerMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_ROOT_WINDOW_CONTROLLER_MUS_H_
