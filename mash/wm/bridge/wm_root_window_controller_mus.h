// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_BRIDGE_WM_ROOT_CONTROLLER_MUS_H_
#define MASH_WM_BRIDGE_WM_ROOT_CONTROLLER_MUS_H_

#include "ash/wm/common/wm_root_window_controller.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace gfx {
class Display;
}

namespace mus {
class Window;
}

namespace mash {
namespace wm {

class RootWindowController;
class WmGlobalsMus;
class WmWindowMus;

// WmRootWindowController implementations for mus.
class WmRootWindowControllerMus : public ash::wm::WmRootWindowController {
 public:
  WmRootWindowControllerMus(WmGlobalsMus* globals,
                            RootWindowController* root_window_controller);
  ~WmRootWindowControllerMus() override;

  static WmRootWindowControllerMus* Get(mus::Window* window) {
    return const_cast<WmRootWindowControllerMus*>(
        Get(const_cast<const mus::Window*>(window)));
  }
  static const WmRootWindowControllerMus* Get(const mus::Window* window);

  RootWindowController* root_window_controller() {
    return root_window_controller_;
  }

  // Screen conversion functions.
  gfx::Point ConvertPointToScreen(const WmWindowMus* source,
                                  const gfx::Point& point) const;
  gfx::Point ConvertPointFromScreen(const WmWindowMus* target,
                                    const gfx::Point& point) const;

  const gfx::Display& GetDisplay() const;

  // WmRootWindowController:
  bool HasShelf() override;
  ash::wm::WmGlobals* GetGlobals() override;
  ash::wm::WorkspaceWindowState GetWorkspaceWindowState() override;
  ash::AlwaysOnTopController* GetAlwaysOnTopController() override;
  ash::wm::WmShelf* GetShelf() override;
  ash::wm::WmWindow* GetWindow() override;
  void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      int shell_container_id,
      views::Widget::InitParams* init_params) override;
  ash::wm::WmWindow* FindEventTarget(
      const gfx::Point& location_in_screen) override;
  void AddObserver(ash::wm::WmRootWindowControllerObserver* observer) override;
  void RemoveObserver(
      ash::wm::WmRootWindowControllerObserver* observer) override;

 private:
  WmGlobalsMus* globals_;
  RootWindowController* root_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(WmRootWindowControllerMus);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_BRIDGE_WM_ROOT_CONTROLLER_MUS_H_
