// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_AURA_WM_ROOT_CONTROLLER_AURA_H_
#define ASH_WM_AURA_WM_ROOT_CONTROLLER_AURA_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/common/wm_root_window_controller.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace aura {
class Window;
}

namespace ash {

class RootWindowController;

namespace wm {

class ASH_EXPORT WmRootWindowControllerAura : public WmRootWindowController,
                                              public ShellObserver {
 public:
  explicit WmRootWindowControllerAura(
      RootWindowController* root_window_controller);
  ~WmRootWindowControllerAura() override;

  static WmRootWindowControllerAura* Get(aura::Window* window) {
    return const_cast<WmRootWindowControllerAura*>(
        Get(const_cast<const aura::Window*>(window)));
  }
  static const WmRootWindowControllerAura* Get(const aura::Window* window);

  // WmRootWindowController:
  bool HasShelf() override;
  WmGlobals* GetGlobals() override;
  WorkspaceWindowState GetWorkspaceWindowState() override;
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

  // ShellObserver:
  void OnDisplayWorkAreaInsetsChanged() override;
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* root_window) override;
  void OnShelfAlignmentChanged(aura::Window* root_window) override;

 private:
  RootWindowController* root_window_controller_;
  base::ObserverList<WmRootWindowControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WmRootWindowControllerAura);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_AURA_WM_ROOT_CONTROLLER_AURA_H_
