// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_MANAGER_H_
#define ASH_MUS_WINDOW_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "services/ui/common/types.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"

namespace base {
class SequencedWorkerPool;
}

namespace display {
class Display;
class ScreenBase;
}

namespace shell {
class Connector;
}

namespace views {
class PointerWatcherEventRouter;
}

namespace ash {
namespace mus {

class AcceleratorHandler;
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
class WindowManager : public ui::WindowManagerDelegate,
                      public ui::WindowTreeClientDelegate {
 public:
  explicit WindowManager(shell::Connector* connector);
  ~WindowManager() override;

  void Init(std::unique_ptr<ui::WindowTreeClient> window_tree_client,
            const scoped_refptr<base::SequencedWorkerPool>& blocking_pool);

  WmShellMus* shell() { return shell_.get(); }

  display::ScreenBase* screen() { return screen_.get(); }

  ui::WindowTreeClient* window_tree_client() {
    return window_tree_client_.get();
  }

  ui::WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

  shell::Connector* connector() { return connector_; }

  void SetScreenLocked(bool is_locked);

  // Creates a new top level window.
  ui::Window* NewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties);

  std::set<RootWindowController*> GetRootWindowControllers();

  // Returns the next accelerator namespace id by value in |id|. Returns true
  // if there is another slot available, false if all slots are taken up.
  bool GetNextAcceleratorNamespaceId(uint16_t* id);
  void AddAcceleratorHandler(uint16_t id_namespace,
                             AcceleratorHandler* handler);
  void RemoveAcceleratorHandler(uint16_t id_namespace);

  void AddObserver(WindowManagerObserver* observer);
  void RemoveObserver(WindowManagerObserver* observer);

 private:
  friend class WmTestHelper;

  using RootWindowControllers = std::set<std::unique_ptr<RootWindowController>>;

  RootWindowController* CreateRootWindowController(
      ui::Window* window,
      const display::Display& display);

  // Deletes the specified RootWindowController. Called when a display is
  // removed.
  void DestroyRootWindowController(
      RootWindowController* root_window_controller);

  void Shutdown();

  // Returns an iterator into |root_window_controllers_|. Returns
  // root_window_controllers_.end() if |window| is not the root of a
  // RootWindowController.
  RootWindowControllers::iterator FindRootWindowControllerByWindow(
      ui::Window* window);

  RootWindowController* GetPrimaryRootWindowController();

  // Returns the RootWindowController where new top levels are created.
  // |properties| is the properties supplied during window creation.
  RootWindowController* GetRootWindowControllerForNewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties);

  // WindowTreeClientDelegate:
  void OnEmbed(ui::Window* root) override;
  void OnEmbedRootDestroyed(ui::Window* root) override;
  void OnLostConnection(ui::WindowTreeClient* client) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              ui::Window* target) override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(ui::WindowManagerClient* client) override;
  bool OnWmSetBounds(ui::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      ui::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  ui::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<ui::Window*>& client_windows,
                                  bool not_responding) override;
  void OnWmNewDisplay(ui::Window* window,
                      const display::Display& display) override;
  void OnWmDisplayRemoved(ui::Window* window) override;
  void OnWmPerformMoveLoop(ui::Window* window,
                           ui::mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(ui::Window* window) override;
  ui::mojom::EventResult OnAccelerator(uint32_t id,
                                       const ui::Event& event) override;

  shell::Connector* connector_;

  std::unique_ptr<ui::WindowTreeClient> window_tree_client_;

  ui::WindowManagerClient* window_manager_client_ = nullptr;

  std::unique_ptr<views::PointerWatcherEventRouter>
      pointer_watcher_event_router_;

  std::unique_ptr<ShadowController> shadow_controller_;

  RootWindowControllers root_window_controllers_;

  base::ObserverList<WindowManagerObserver> observers_;

  std::unique_ptr<display::ScreenBase> screen_;

  std::unique_ptr<WmShellMus> shell_;

  std::unique_ptr<WmLookupMus> lookup_;

  std::map<uint16_t, AcceleratorHandler*> accelerator_handlers_;
  uint16_t next_accelerator_namespace_id_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_H_
