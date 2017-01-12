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
#include "base/observer_list.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/display/display_controller.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_tree_client_delegate.h"

namespace aura {
namespace client {
class ActivationClient;
}
}

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
class FocusController;
class WMState;
}

namespace ash {

class EventClientImpl;
class ScreenPositionController;
class ScreenMus;

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
class WindowManager : public aura::WindowManagerDelegate,
                      public aura::WindowTreeClientDelegate {
 public:
  explicit WindowManager(service_manager::Connector* connector);
  ~WindowManager() override;

  void Init(std::unique_ptr<aura::WindowTreeClient> window_tree_client,
            const scoped_refptr<base::SequencedWorkerPool>& blocking_pool);

  WmShellMus* shell() { return shell_.get(); }

  ScreenMus* screen() { return screen_.get(); }

  aura::WindowTreeClient* window_tree_client() {
    return window_tree_client_.get();
  }

  aura::WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

  ::wm::FocusController* focus_controller() { return focus_controller_.get(); }

  aura::client::ActivationClient* activation_client();

  service_manager::Connector* connector() { return connector_; }

  aura::PropertyConverter* property_converter() {
    return property_converter_.get();
  }

  // Creates a new top level window.
  aura::Window* NewTopLevelWindow(
      ui::mojom::WindowType window_type,
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

  // Returns the DisplayController interface if available. Will be null if no
  // service_manager::Connector was available, for example in some tests.
  display::mojom::DisplayController* GetDisplayController();

 private:
  friend class WmTestHelper;

  using RootWindowControllers = std::set<std::unique_ptr<RootWindowController>>;

  RootWindowController* CreateRootWindowController(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
      const display::Display& display,
      ash::RootWindowController::RootWindowType root_window_type);

  // Deletes the specified RootWindowController. Called when a display is
  // removed.
  void DestroyRootWindowController(
      RootWindowController* root_window_controller);

  void Shutdown();

  RootWindowController* GetPrimaryRootWindowController();

  // Returns the RootWindowController where new top levels are created.
  // |properties| is the properties supplied during window creation.
  RootWindowController* GetRootWindowControllerForNewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties);

  // WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override;
  void OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) override;
  void OnLostConnection(aura::WindowTreeClient* client) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override;
  aura::client::CaptureClient* GetCaptureClient() override;
  aura::PropertyConverter* GetPropertyConverter() override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(aura::WindowManagerClient* client) override;
  bool OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      aura::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
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

  service_manager::Connector* connector_;
  display::mojom::DisplayControllerPtr display_controller_;

  std::unique_ptr<::wm::FocusController> focus_controller_;
  std::unique_ptr<::wm::WMState> wm_state_;
  std::unique_ptr<aura::PropertyConverter> property_converter_;

  std::unique_ptr<aura::WindowTreeClient> window_tree_client_;

  aura::WindowManagerClient* window_manager_client_ = nullptr;

  std::unique_ptr<views::PointerWatcherEventRouter>
      pointer_watcher_event_router_;

  std::unique_ptr<ShadowController> shadow_controller_;

  RootWindowControllers root_window_controllers_;

  base::ObserverList<WindowManagerObserver> observers_;

  std::unique_ptr<ScreenMus> screen_;

  std::unique_ptr<WmShellMus> shell_;

  std::unique_ptr<WmLookupMus> lookup_;

  std::map<uint16_t, AcceleratorHandler*> accelerator_handlers_;
  uint16_t next_accelerator_namespace_id_ = 0u;

  std::unique_ptr<EventClientImpl> event_client_;

  std::unique_ptr<ScreenPositionController> screen_position_controller_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_H_
