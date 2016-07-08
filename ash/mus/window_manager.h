// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_MANAGER_H_
#define ASH_MUS_WINDOW_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/observer_list.h"
#include "services/ui/common/types.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_observer.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"

namespace display {
class Display;
class Screen;
}

namespace shell {
class Connector;
}

namespace ash {
namespace mus {

class RootWindowController;
class ShadowController;
class WindowManagerObserver;
class WmShellMus;
class WmLookupMus;
class WmTestHelper;

// WindowManager serves as the WindowManagerDelegate and
// WindowTreeClientDelegate for mash. WindowManager creates (and owns)
// a RootWindowController per Display. WindowManager takes ownership of
// the WindowTreeClient.
class WindowManager : public ::ui::WindowManagerDelegate,
                      public ::ui::WindowObserver,
                      public ::ui::WindowTreeClientDelegate {
 public:
  explicit WindowManager(shell::Connector* connector);
  ~WindowManager() override;

  void Init(::ui::WindowTreeClient* window_tree_client);

  WmShellMus* shell() { return shell_.get(); }

  ::ui::WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

  shell::Connector* connector() { return connector_; }

  void SetScreenLocked(bool is_locked);

  // Creates a new top level window.
  ::ui::Window* NewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties);

  std::set<RootWindowController*> GetRootWindowControllers();

  void AddObserver(WindowManagerObserver* observer);
  void RemoveObserver(WindowManagerObserver* observer);

 private:
  friend class WmTestHelper;

  void AddAccelerators();

  RootWindowController* CreateRootWindowController(
      ::ui::Window* window,
      const display::Display& display);

  // ::ui::WindowObserver:
  void OnWindowDestroying(::ui::Window* window) override;
  void OnWindowDestroyed(::ui::Window* window) override;

  // WindowTreeClientDelegate:
  void OnEmbed(::ui::Window* root) override;
  void OnDidDestroyClient(::ui::WindowTreeClient* client) override;
  void OnEventObserved(const ui::Event& event, ::ui::Window* target) override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(::ui::WindowManagerClient* client) override;
  bool OnWmSetBounds(::ui::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      ::ui::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  ::ui::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<::ui::Window*>& client_windows,
                                  bool not_responding) override;
  void OnWmNewDisplay(::ui::Window* window,
                      const display::Display& display) override;
  void OnWmPerformMoveLoop(::ui::Window* window,
                           ::ui::mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(::ui::Window* window) override;
  ui::mojom::EventResult OnAccelerator(uint32_t id,
                                       const ui::Event& event) override;

  shell::Connector* connector_;

  ::ui::WindowTreeClient* window_tree_client_ = nullptr;

  ::ui::WindowManagerClient* window_manager_client_ = nullptr;

  std::unique_ptr<ShadowController> shadow_controller_;

  std::set<std::unique_ptr<RootWindowController>> root_window_controllers_;

  base::ObserverList<WindowManagerObserver> observers_;

  std::unique_ptr<display::Screen> screen_;

  std::unique_ptr<WmShellMus> shell_;

  std::unique_ptr<WmLookupMus> lookup_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_H_
