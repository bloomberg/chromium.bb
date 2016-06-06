// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/root_window_controller.h"

#include <stdint.h>

#include <map>
#include <sstream>

#include "ash/common/root_window_controller_common.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/always_on_top_controller.h"
#include "ash/common/wm/dock/docked_window_layout_manager.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/root_window_layout_manager.h"
#include "ash/common/wm/workspace/workspace_layout_manager.h"
#include "ash/common/wm/workspace/workspace_layout_manager_delegate.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_shelf_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/container_ids.h"
#include "ash/mus/screenlock_layout.h"
#include "ash/mus/shadow_controller.h"
#include "ash/mus/shelf_layout_manager.h"
#include "ash/mus/status_layout_manager.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "components/mus/common/event_matcher_util.h"
#include "components/mus/common/switches.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/cpp/window_tree_host_factory.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/display/mojo/display_type_converters.h"

using ash::mojom::Container;

namespace ash {
namespace mus {
namespace {

const uint32_t kWindowSwitchAccelerator = 1;

void AssertTrue(bool success) {
  DCHECK(success);
}

class WorkspaceLayoutManagerDelegateImpl
    : public wm::WorkspaceLayoutManagerDelegate {
 public:
  explicit WorkspaceLayoutManagerDelegateImpl(
      WmRootWindowControllerMus* root_window_controller)
      : root_window_controller_(root_window_controller) {}
  ~WorkspaceLayoutManagerDelegateImpl() override = default;

  // WorkspaceLayoutManagerDelegate:
  void UpdateShelfVisibility() override { NOTIMPLEMENTED(); }
  void OnFullscreenStateChanged(bool is_fullscreen) override {
    // TODO(sky): this should only do something if there is a shelf, see
    // implementation in ash/shell.cc.
    NOTIMPLEMENTED();
    root_window_controller_->NotifyFullscreenStateChange(is_fullscreen);
  }

 private:
  WmRootWindowControllerMus* root_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerDelegateImpl);
};

}  // namespace

// static
RootWindowController* RootWindowController::CreateFromDisplay(
    WindowManagerApplication* app,
    ::mus::mojom::DisplayPtr display,
    ::mus::mojom::WindowTreeClientRequest client_request) {
  RootWindowController* controller = new RootWindowController(app);
  controller->display_ = display.To<display::Display>();
  new ::mus::WindowTreeClient(controller, controller->window_manager_.get(),
                              std::move(client_request));
  return controller;
}

void RootWindowController::Destroy() {
  // See class description for details on lifetime.
  if (root_) {
    delete root_->window_tree();
  } else {
    // This case only happens if we're destroyed before OnEmbed().
    delete this;
  }
}

shell::Connector* RootWindowController::GetConnector() {
  return app_->connector();
}

::mus::Window* RootWindowController::GetWindowForContainer(
    Container container) {
  WmWindowMus* wm_window =
      GetWindowByShellWindowId(MashContainerToAshShellWindowId(container));
  DCHECK(wm_window);
  return wm_window->mus_window();
}

bool RootWindowController::WindowIsContainer(::mus::Window* window) {
  return window &&
         WmWindowMus::Get(window)->GetShellWindowId() != kShellWindowId_Invalid;
}

WmWindowMus* RootWindowController::GetWindowByShellWindowId(int id) {
  return WmWindowMus::AsWmWindowMus(
      WmWindowMus::Get(root_)->GetChildByShellWindowId(id));
}

::mus::WindowManagerClient* RootWindowController::window_manager_client() {
  return window_manager_->window_manager_client();
}

void RootWindowController::OnAccelerator(uint32_t id, const ui::Event& event) {
  switch (id) {
    case kWindowSwitchAccelerator:
      window_manager_client()->ActivateNextWindow();
      break;
    default:
      app_->OnAccelerator(id, event);
      break;
  }
}

ShelfLayoutManager* RootWindowController::GetShelfLayoutManager() {
  return static_cast<ShelfLayoutManager*>(
      layout_managers_[GetWindowForContainer(Container::USER_PRIVATE_SHELF)]
          .get());
}

StatusLayoutManager* RootWindowController::GetStatusLayoutManager() {
  return static_cast<StatusLayoutManager*>(
      layout_managers_[GetWindowForContainer(Container::STATUS)].get());
}

RootWindowController::RootWindowController(WindowManagerApplication* app)
    : app_(app), root_(nullptr), window_count_(0) {
  window_manager_.reset(new WindowManager);
}

RootWindowController::~RootWindowController() {}

void RootWindowController::AddAccelerators() {
  window_manager_client()->AddAccelerator(
      kWindowSwitchAccelerator,
      ::mus::CreateKeyMatcher(::mus::mojom::KeyboardCode::TAB,
                              ::mus::mojom::kEventFlagControlDown),
      base::Bind(&AssertTrue));
}

void RootWindowController::OnEmbed(::mus::Window* root) {
  root_ = root;
  root_->AddObserver(this);

  app_->OnRootWindowControllerGotRoot(this);

  wm_root_window_controller_.reset(
      new WmRootWindowControllerMus(app_->shell(), this));

  root_window_controller_common_.reset(
      new RootWindowControllerCommon(WmWindowMus::Get(root_)));
  root_window_controller_common_->CreateContainers();
  root_window_controller_common_->CreateLayoutManagers();
  CreateLayoutManagers();

  // Force a layout of the root, and its children, RootWindowLayout handles
  // both.
  root_window_controller_common_->root_window_layout()->OnWindowResized();

  for (size_t i = 0; i < kNumActivatableShellWindowIds; ++i) {
    window_manager_client()->AddActivationParent(
        GetWindowByShellWindowId(kActivatableShellWindowIds[i])->mus_window());
  }

  WmWindowMus* always_on_top_container =
      GetWindowByShellWindowId(kShellWindowId_AlwaysOnTopContainer);
  always_on_top_controller_.reset(
      new AlwaysOnTopController(always_on_top_container));

  AddAccelerators();

  window_manager_->Initialize(this, app_->session());

  shadow_controller_.reset(new ShadowController(root->window_tree()));

  app_->OnRootWindowControllerDoneInit(this);
}

void RootWindowController::OnWindowTreeClientDestroyed(
    ::mus::WindowTreeClient* client) {
  shadow_controller_.reset();
  delete this;
}

void RootWindowController::OnEventObserved(const ui::Event& event,
                                           ::mus::Window* target) {
  // Does not use EventObservers.
}

void RootWindowController::OnWindowDestroyed(::mus::Window* window) {
  DCHECK_EQ(window, root_);
  app_->OnRootWindowDestroyed(this);
  root_->RemoveObserver(this);
  // Delete the |window_manager_| here so that WindowManager doesn't have to
  // worry about the possibility of |root_| being null.
  window_manager_.reset();
  root_ = nullptr;
}

void RootWindowController::OnShelfWindowAvailable() {
  DockedWindowLayoutManager::Get(WmWindowMus::Get(root_))
      ->SetShelf(wm_shelf_.get());

  PanelLayoutManager::Get(WmWindowMus::Get(root_))->SetShelf(wm_shelf_.get());

  // TODO: http://crbug.com/614182 Ash's ShelfLayoutManager implements
  // DockedWindowLayoutManagerObserver so that it can inset by the docked
  // windows.
  // docked_layout_manager_->AddObserver(shelf_->shelf_layout_manager());
}

void RootWindowController::CreateLayoutManagers() {
  // Override the default layout managers for certain containers.
  WmWindowMus* lock_screen_container =
      GetWindowByShellWindowId(kShellWindowId_LockScreenContainer);
  layout_managers_[lock_screen_container->mus_window()].reset(
      new ScreenlockLayout(lock_screen_container->mus_window()));

  WmWindowMus* shelf_container =
      GetWindowByShellWindowId(kShellWindowId_ShelfContainer);
  ShelfLayoutManager* shelf_layout_manager =
      new ShelfLayoutManager(shelf_container->mus_window(), this);
  layout_managers_[shelf_container->mus_window()].reset(shelf_layout_manager);

  wm_shelf_.reset(new WmShelfMus(shelf_layout_manager));

  WmWindowMus* status_container =
      GetWindowByShellWindowId(kShellWindowId_StatusContainer);
  layout_managers_[status_container->mus_window()].reset(
      new StatusLayoutManager(status_container->mus_window()));

  WmWindowMus* default_container =
      GetWindowByShellWindowId(kShellWindowId_DefaultContainer);
  // WorkspaceLayoutManager is not a mash::wm::LayoutManager (it's a
  // wm::LayoutManager), so it can't be in |layout_managers_|.
  std::unique_ptr<WorkspaceLayoutManagerDelegateImpl>
      workspace_layout_manager_delegate(new WorkspaceLayoutManagerDelegateImpl(
          wm_root_window_controller_.get()));
  default_container->SetLayoutManager(
      base::WrapUnique(new WorkspaceLayoutManager(
          default_container, std::move(workspace_layout_manager_delegate))));

  WmWindowMus* docked_container =
      GetWindowByShellWindowId(kShellWindowId_DockedContainer);
  docked_container->SetLayoutManager(
      base::WrapUnique(new DockedWindowLayoutManager(docked_container)));

  WmWindowMus* panel_container =
      GetWindowByShellWindowId(kShellWindowId_PanelContainer);
  panel_container->SetLayoutManager(
      base::WrapUnique(new PanelLayoutManager(panel_container)));
}

}  // namespace mus
}  // namespace ash
