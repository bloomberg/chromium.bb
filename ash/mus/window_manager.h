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
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_client_delegate.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"

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
class WindowManager : public ::mus::WindowManagerDelegate,
                      public ::mus::WindowObserver,
                      public ::mus::WindowTreeClientDelegate {
 public:
  explicit WindowManager(shell::Connector* connector);
  ~WindowManager() override;

  void Init(::mus::WindowTreeClient* window_tree_client);

  WmShellMus* shell() { return shell_.get(); }

  ::mus::WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

  shell::Connector* connector() { return connector_; }

  void SetScreenLocked(bool is_locked);

  // Creates a new top level window.
  ::mus::Window* NewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties);

  std::set<RootWindowController*> GetRootWindowControllers();

  void AddObserver(WindowManagerObserver* observer);
  void RemoveObserver(WindowManagerObserver* observer);

 private:
  friend class WmTestHelper;

  void AddAccelerators();

  RootWindowController* CreateRootWindowController(
      ::mus::Window* window,
      const display::Display& display);

  // ::mus::WindowObserver:
  void OnWindowDestroying(::mus::Window* window) override;
  void OnWindowDestroyed(::mus::Window* window) override;

  // WindowTreeClientDelegate:
  void OnEmbed(::mus::Window* root) override;
  void OnWindowTreeClientDestroyed(::mus::WindowTreeClient* client) override;
  void OnEventObserved(const ui::Event& event, ::mus::Window* target) override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(::mus::WindowManagerClient* client) override;
  bool OnWmSetBounds(::mus::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      ::mus::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  ::mus::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(
      const std::set<::mus::Window*>& client_windows,
      bool not_responding) override;
  void OnWmNewDisplay(::mus::Window* window,
                      const display::Display& display) override;
  void OnAccelerator(uint32_t id, const ui::Event& event) override;

  shell::Connector* connector_;

  ::mus::WindowTreeClient* window_tree_client_ = nullptr;

  ::mus::WindowManagerClient* window_manager_client_ = nullptr;

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
