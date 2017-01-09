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
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/dock/docked_window_layout_manager.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/root_window_layout_manager.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/non_client_frame_controller.h"
#include "ash/mus/property_util.h"
#include "ash/mus/screen_mus.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/stacking_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/common/switches.h"
#include "services/ui/common/util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display_list.h"

namespace ash {
namespace mus {
namespace {

bool IsFullscreen(aura::PropertyConverter* property_converter,
                  const std::vector<uint8_t>& transport_data) {
  using ui::mojom::WindowManager;
  aura::PropertyConverter::PrimitiveType show_state = 0;
  return property_converter->GetPropertyValueFromTransportValue(
             WindowManager::kShowState_Property, transport_data, &show_state) &&
         (static_cast<ui::WindowShowState>(show_state) ==
          ui::SHOW_STATE_FULLSCREEN);
}

}  // namespace

RootWindowController::RootWindowController(
    WindowManager* window_manager,
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
    const display::Display& display)
    : window_manager_(window_manager),
      window_tree_host_(std::move(window_tree_host)),
      window_count_(0),
      display_(display),
      wm_shelf_(base::MakeUnique<WmShelf>()) {
  wm_root_window_controller_ = base::MakeUnique<WmRootWindowControllerMus>(
      window_manager_->shell(), this);
  wm_root_window_controller_->CreateContainers();
  wm_root_window_controller_->CreateLayoutManagers();

  parenting_client_ = base::MakeUnique<StackingController>();
  aura::client::SetWindowParentingClient(root(), parenting_client_.get());

  disconnected_app_handler_.reset(new DisconnectedAppHandler(root()));

  // Force a layout of the root, and its children, RootWindowLayout handles
  // both.
  wm_root_window_controller_->root_window_layout_manager()->OnWindowResized();

  for (size_t i = 0; i < kNumActivatableShellWindowIds; ++i) {
    window_manager_->window_manager_client()->AddActivationParent(
        GetWindowByShellWindowId(kActivatableShellWindowIds[i])->aura_window());
  }
}

RootWindowController::~RootWindowController() {
  Shutdown();
  // Destroy WindowTreeHost last as it owns the root Window.
  wm_shelf_.reset();
  wm_root_window_controller_.reset();
  window_tree_host_.reset();
}

void RootWindowController::Shutdown() {
  // NOTE: Shutdown() may be called multiple times.
  wm_root_window_controller_->ResetRootForNewWindowsIfNecessary();
  wm_root_window_controller_->CloseChildWindows();
}

service_manager::Connector* RootWindowController::GetConnector() {
  return window_manager_->connector();
}

aura::Window* RootWindowController::root() {
  return window_tree_host_->window();
}

const aura::Window* RootWindowController::root() const {
  return window_tree_host_->window();
}

aura::Window* RootWindowController::NewTopLevelWindow(
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  // TODO(sky): constrain and validate properties.

  int32_t container_id = kShellWindowId_Invalid;
  aura::Window* context = nullptr;
  aura::Window* container_window = nullptr;
  if (GetInitialContainerId(*properties, &container_id))
    container_window = GetWindowByShellWindowId(container_id)->aura_window();
  else
    context = root();

  gfx::Rect bounds = CalculateDefaultBounds(container_window, properties);
  window_count_++;

  const bool provide_non_client_frame =
      window_type == ui::mojom::WindowType::WINDOW ||
      window_type == ui::mojom::WindowType::PANEL;
  if (provide_non_client_frame) {
    (*properties)[ui::mojom::kWaitForUnderlay_Property].clear();
    // See NonClientFrameController for details on lifetime.
    NonClientFrameController* non_client_frame_controller =
        new NonClientFrameController(container_window, context, bounds,
                                     window_type, properties, window_manager_);
    return non_client_frame_controller->window();
  }

  aura::Window* window = new aura::Window(nullptr);
  aura::SetWindowType(window, window_type);
  // Apply properties before Init(), that way they are sent to the server at
  // the time the window is created.
  aura::PropertyConverter* property_converter =
      window_manager_->property_converter();
  for (auto& property_pair : *properties) {
    property_converter->SetPropertyFromTransportValue(
        window, property_pair.first, &property_pair.second);
  }
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(bounds);

  if (container_window) {
    container_window->AddChild(window);
  } else {
    WmWindowMus* root = WmWindowMus::Get(this->root());
    gfx::Point origin =
        wm_root_window_controller_->ConvertPointToScreen(root, gfx::Point());
    gfx::Rect bounds_in_screen(origin, bounds.size());
    static_cast<WmWindowMus*>(
        ash::wm::GetDefaultParent(WmWindowMus::Get(context),
                                  WmWindowMus::Get(window), bounds_in_screen))
        ->aura_window()
        ->AddChild(window);
  }
  return window;
}

WmWindowMus* RootWindowController::GetWindowByShellWindowId(int id) {
  return WmWindowMus::AsWmWindowMus(
      WmWindowMus::Get(root())->GetChildByShellWindowId(id));
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
    aura::Window* container_window,
    const std::map<std::string, std::vector<uint8_t>>* properties) const {
  gfx::Rect requested_bounds;
  if (GetInitialBounds(*properties, &requested_bounds))
    return requested_bounds;

  auto show_state_iter =
      properties->find(ui::mojom::WindowManager::kShowState_Property);
  if (show_state_iter != properties->end()) {
    if (IsFullscreen(window_manager_->property_converter(),
                     show_state_iter->second)) {
      gfx::Rect bounds(0, 0, root()->bounds().width(),
                       root()->bounds().height());
      if (!container_window) {
        bounds.Offset(display_.bounds().origin().x(),
                      display_.bounds().origin().y());
      }
      return bounds;
    }
  }

  int width, height;
  gfx::Size pref;
  if (GetWindowPreferredSize(*properties, &pref) && !pref.IsEmpty()) {
    // TODO(sky): likely want to constrain more than root size.
    const gfx::Size max_size = root()->bounds().size();
    width = std::max(0, std::min(max_size.width(), pref.width()));
    height = std::max(0, std::min(max_size.height(), pref.height()));
  } else {
    width = root()->bounds().width() - 240;
    height = root()->bounds().height() - 240;
  }
  return gfx::Rect(40 + (window_count_ % 4) * 40, 40 + (window_count_ % 4) * 40,
                   width, height);
}

}  // namespace mus
}  // namespace ash
