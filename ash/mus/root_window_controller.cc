// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/root_window_controller.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/dock/docked_window_layout_manager.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/root_window_layout_manager.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_shelf_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/non_client_frame_controller.h"
#include "ash/mus/property_util.h"
#include "ash/mus/screenlock_layout.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/common/switches.h"
#include "services/ui/common/util.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/display/display_list.h"
#include "ui/display/screen_base.h"

namespace ash {
namespace mus {

RootWindowController::RootWindowController(WindowManager* window_manager,
                                           ui::Window* root,
                                           const display::Display& display)
    : window_manager_(window_manager),
      root_(root),
      window_count_(0),
      display_(display),
      wm_shelf_(base::MakeUnique<WmShelfMus>()) {
  wm_root_window_controller_ = base::MakeUnique<WmRootWindowControllerMus>(
      window_manager_->shell(), this);
  wm_root_window_controller_->CreateContainers();
  wm_root_window_controller_->CreateLayoutManagers();
  CreateLayoutManagers();

  disconnected_app_handler_.reset(new DisconnectedAppHandler(root));

  // Force a layout of the root, and its children, RootWindowLayout handles
  // both.
  wm_root_window_controller_->root_window_layout_manager()->OnWindowResized();

  for (size_t i = 0; i < kNumActivatableShellWindowIds; ++i) {
    window_manager_->window_manager_client()->AddActivationParent(
        GetWindowByShellWindowId(kActivatableShellWindowIds[i])->mus_window());
  }
}

RootWindowController::~RootWindowController() {
  Shutdown();
  root_->Destroy();
}

void RootWindowController::Shutdown() {
  // NOTE: Shutdown() may be called multiple times.
  wm_root_window_controller_->ResetRootForNewWindowsIfNecessary();
  wm_root_window_controller_->CloseChildWindows();
}

service_manager::Connector* RootWindowController::GetConnector() {
  return window_manager_->connector();
}

ui::Window* RootWindowController::NewTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  const bool provide_non_client_frame =
      GetWindowType(*properties) == ui::mojom::WindowType::WINDOW ||
      GetWindowType(*properties) == ui::mojom::WindowType::PANEL;
  if (provide_non_client_frame)
    (*properties)[ui::mojom::kWaitForUnderlay_Property].clear();

  // TODO(sky): constrain and validate properties before passing to server.
  ui::Window* window = root_->window_tree()->NewWindow(properties);
  window->SetBounds(CalculateDefaultBounds(window));

  ui::Window* container_window = nullptr;
  int container_id = kShellWindowId_Invalid;
  if (GetRequestedContainer(window, &container_id)) {
    container_window = GetWindowByShellWindowId(container_id)->mus_window();
  } else {
    gfx::Point origin = wm_root_window_controller_->ConvertPointToScreen(
        WmWindowMus::Get(root_), gfx::Point());
    gfx::Rect bounds_in_screen(origin, window->bounds().size());
    container_window = WmWindowMus::GetMusWindow(wm::GetDefaultParent(
        WmWindowMus::Get(root_), WmWindowMus::Get(window), bounds_in_screen));
  }
  DCHECK(WmWindowMus::Get(container_window)->IsContainer());

  if (provide_non_client_frame) {
    NonClientFrameController::Create(container_window, window,
                                     window_manager_->window_manager_client());
  } else {
    container_window->AddChild(window);
  }

  window_count_++;

  return window;
}

WmWindowMus* RootWindowController::GetWindowByShellWindowId(int id) {
  return WmWindowMus::AsWmWindowMus(
      WmWindowMus::Get(root_)->GetChildByShellWindowId(id));
}

void RootWindowController::SetWorkAreaInests(const gfx::Insets& insets) {
  gfx::Rect old_work_area = display_.work_area();
  display_.UpdateWorkAreaFromInsets(insets);

  if (old_work_area == display_.work_area())
    return;

  window_manager_->screen()->display_list().UpdateDisplay(display_);

  // Push new display insets to service:ui if we have a connection.
  auto* display_controller = window_manager_->GetDisplayController();
  if (display_controller)
    display_controller->SetDisplayWorkArea(display_.id(),
                                           display_.bounds().size(), insets);
}

void RootWindowController::SetDisplay(const display::Display& display) {
  DCHECK_EQ(display.id(), display_.id());
  display_ = display;
  window_manager_->screen()->display_list().UpdateDisplay(display_);
}

gfx::Rect RootWindowController::CalculateDefaultBounds(
    ui::Window* window) const {
  if (window->HasSharedProperty(
          ui::mojom::WindowManager::kInitialBounds_Property)) {
    return window->GetSharedProperty<gfx::Rect>(
        ui::mojom::WindowManager::kInitialBounds_Property);
  }

  if (GetWindowShowState(window) == ui::mojom::ShowState::FULLSCREEN) {
    return gfx::Rect(0, 0, root_->bounds().width(), root_->bounds().height());
  }

  int width, height;
  const gfx::Size pref = GetWindowPreferredSize(window);
  if (pref.IsEmpty()) {
    width = root_->bounds().width() - 240;
    height = root_->bounds().height() - 240;
  } else {
    // TODO(sky): likely want to constrain more than root size.
    const gfx::Size max_size = root_->bounds().size();
    width = std::max(0, std::min(max_size.width(), pref.width()));
    height = std::max(0, std::min(max_size.height(), pref.height()));
  }
  return gfx::Rect(40 + (window_count_ % 4) * 40, 40 + (window_count_ % 4) * 40,
                   width, height);
}

void RootWindowController::CreateLayoutManagers() {
  // Override the default layout managers for certain containers.
  WmWindowMus* lock_screen_container =
      GetWindowByShellWindowId(kShellWindowId_LockScreenContainer);
  layout_managers_[lock_screen_container->mus_window()].reset(
      new ScreenlockLayout(lock_screen_container->mus_window()));
}

}  // namespace mus
}  // namespace ash
