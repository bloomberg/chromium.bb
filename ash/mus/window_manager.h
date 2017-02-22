// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_MANAGER_H_
#define ASH_MUS_WINDOW_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "ash/root_window_controller.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/display/display_controller.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_tree_client_delegate.h"

namespace base {
class SequencedWorkerPool;
}

namespace display {
class Display;
}

namespace service_manager {
class Connector;
}

namespace views {
class PointerWatcherEventRouter;
}

namespace wm {
class WMState;
}

namespace ash {

class RootWindowController;
class ScreenMus;
class ShellDelegate;

namespace test {
class AshTestHelper;
}

namespace mus {

class AcceleratorHandler;
class WmLookupMus;
class WmTestHelper;

// WindowManager serves as the WindowManagerDelegate and
// WindowTreeClientDelegate for mash. WindowManager creates (and owns)
// a RootWindowController per Display. WindowManager takes ownership of
// the WindowTreeClient.
class WindowManager : public aura::WindowManagerDelegate,
                      public aura::WindowTreeClientDelegate {
 public:
  explicit WindowManager(service_manager::Connector* connector);
  ~WindowManager() override;

  void Init(std::unique_ptr<aura::WindowTreeClient> window_tree_client,
            const scoped_refptr<base::SequencedWorkerPool>& blocking_pool);

  // Called during shutdown to delete all the RootWindowControllers.
  void DeleteAllRootWindowControllers();

  ScreenMus* screen() { return screen_.get(); }

  aura::WindowTreeClient* window_tree_client() {
    return window_tree_client_.get();
  }

  aura::WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

  service_manager::Connector* connector() { return connector_; }

  aura::PropertyConverter* property_converter() {
    return property_converter_.get();
  }

  std::set<RootWindowController*> GetRootWindowControllers();

  // Returns the next accelerator namespace id by value in |id|. Returns true
  // if there is another slot available, false if all slots are taken up.
  bool GetNextAcceleratorNamespaceId(uint16_t* id);
  void AddAcceleratorHandler(uint16_t id_namespace,
                             AcceleratorHandler* handler);
  void RemoveAcceleratorHandler(uint16_t id_namespace);

  // Returns the DisplayController interface if available. Will be null if no
  // service_manager::Connector was available, for example in some tests.
  display::mojom::DisplayController* GetDisplayController();

  // Called during creation of the shell to create a RootWindowController.
  // See comment in CreateRootWindowController() for details.
  void CreatePrimaryRootWindowController(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host);

 private:
  friend class ash::test::AshTestHelper;
  friend class WmTestHelper;

  using RootWindowControllers = std::set<std::unique_ptr<RootWindowController>>;

  // Called once the first Display has been obtained.
  void CreateShell(
      std::unique_ptr<aura::WindowTreeHostMus> primary_window_tree_host);

  void CreateAndRegisterRootWindowController(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
      const display::Display& display,
      ash::RootWindowController::RootWindowType root_window_type);

  // Deletes the specified RootWindowController. Called when a display is
  // removed. |in_shutdown| is true if called from Shutdown().
  void DestroyRootWindowController(RootWindowController* root_window_controller,
                                   bool in_shutdown);

  void Shutdown();

  RootWindowController* GetPrimaryRootWindowController();

  // WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override;
  void OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) override;
  void OnLostConnection(aura::WindowTreeClient* client) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override;
  aura::PropertyConverter* GetPropertyConverter() override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(aura::WindowManagerClient* client) override;
  bool OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      aura::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  void OnWmSetCanFocus(aura::Window* window, bool can_focus) override;
  aura::Window* OnWmCreateTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<aura::Window*>& client_windows,
                                  bool not_responding) override;
  void OnWmWillCreateDisplay(const display::Display& display) override;
  void OnWmNewDisplay(std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
                      const display::Display& display) override;
  void OnWmDisplayRemoved(aura::WindowTreeHostMus* window_tree_host) override;
  void OnWmDisplayModified(const display::Display& display) override;
  void OnWmPerformMoveLoop(aura::Window* window,
                           ui::mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(aura::Window* window) override;
  ui::mojom::EventResult OnAccelerator(uint32_t id,
                                       const ui::Event& event) override;
  void OnWmSetClientArea(
      aura::Window* window,
      const gfx::Insets& insets,
      const std::vector<gfx::Rect>& additional_client_areas) override;
  bool IsWindowActive(aura::Window* window) override;
  void OnWmDeactivateWindow(aura::Window* window) override;

  service_manager::Connector* connector_;
  display::mojom::DisplayControllerPtr display_controller_;

  std::unique_ptr<::wm::WMState> wm_state_;
  std::unique_ptr<aura::PropertyConverter> property_converter_;

  std::unique_ptr<aura::WindowTreeClient> window_tree_client_;

  aura::WindowManagerClient* window_manager_client_ = nullptr;

  std::unique_ptr<views::PointerWatcherEventRouter>
      pointer_watcher_event_router_;

  RootWindowControllers root_window_controllers_;

  std::unique_ptr<ScreenMus> screen_;

  bool created_shell_ = false;

  std::unique_ptr<WmLookupMus> lookup_;

  std::map<uint16_t, AcceleratorHandler*> accelerator_handlers_;
  uint16_t next_accelerator_namespace_id_ = 0u;

  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  // Only set in tests. If non-null this is used as the shell delegate.
  std::unique_ptr<ShellDelegate> shell_delegate_for_test_;

  // See WmShellMus's constructor for details. Tests may set to false.
  bool create_session_state_delegate_stub_for_test_ = true;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_H_
