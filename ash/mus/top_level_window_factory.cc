// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/top_level_window_factory.h"

#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/mus/disconnected_app_handler.h"
#include "ash/mus/non_client_frame_controller.h"
#include "ash/mus/property_util.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"

namespace ash {
namespace mus {
namespace {

// Returns true if a fullscreen window was requested.
bool IsFullscreen(aura::PropertyConverter* property_converter,
                  const std::vector<uint8_t>& transport_data) {
  using ui::mojom::WindowManager;
  aura::PropertyConverter::PrimitiveType show_state = 0;
  return property_converter->GetPropertyValueFromTransportValue(
             WindowManager::kShowState_Property, transport_data, &show_state) &&
         (static_cast<ui::WindowShowState>(show_state) ==
          ui::SHOW_STATE_FULLSCREEN);
}

// Returns the RootWindowController where new top levels are created.
// |properties| is the properties supplied during window creation.
RootWindowController* GetRootWindowControllerForNewTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  // If a specific display was requested, use it.
  const int64_t display_id = GetInitialDisplayId(*properties);
  if (display_id != display::kInvalidDisplayId) {
    for (RootWindowController* root_window_controller :
         RootWindowController::root_window_controllers()) {
      if (GetRootWindowSettings(root_window_controller->GetRootWindow())
              ->display_id == display_id) {
        return root_window_controller;
      }
    }
  }
  return RootWindowController::ForWindow(
      WmShell::Get()->GetRootWindowForNewWindows()->aura_window());
}

// Returns the bounds for the new window.
gfx::Rect CalculateDefaultBounds(
    WindowManager* window_manager,
    RootWindowController* root_window_controller,
    aura::Window* container_window,
    const std::map<std::string, std::vector<uint8_t>>* properties) {
  gfx::Rect requested_bounds;
  if (GetInitialBounds(*properties, &requested_bounds))
    return requested_bounds;

  const gfx::Size root_size =
      root_window_controller->GetRootWindow()->bounds().size();
  auto show_state_iter =
      properties->find(ui::mojom::WindowManager::kShowState_Property);
  if (show_state_iter != properties->end()) {
    if (IsFullscreen(window_manager->property_converter(),
                     show_state_iter->second)) {
      gfx::Rect bounds(root_size);
      if (!container_window) {
        const display::Display display =
            root_window_controller->GetWindow()->GetDisplayNearestWindow();
        bounds.Offset(display.bounds().OffsetFromOrigin());
      }
      return bounds;
    }
  }

  gfx::Size window_size;
  if (GetWindowPreferredSize(*properties, &window_size) &&
      !window_size.IsEmpty()) {
    // TODO(sky): likely want to constrain more than root size.
    window_size.SetToMin(root_size);
  } else {
    static constexpr int kRootSizeDelta = 240;
    window_size.SetSize(root_size.width() - kRootSizeDelta,
                        root_size.height() - kRootSizeDelta);
  }
  // TODO(sky): this should use code in chrome/browser/ui/window_sizer.
  static constexpr int kOriginOffset = 40;
  return gfx::Rect(gfx::Point(kOriginOffset, kOriginOffset), window_size);
}

// Does the real work of CreateAndParentTopLevelWindow() once the appropriate
// RootWindowController was found.
aura::Window* CreateAndParentTopLevelWindowInRoot(
    WindowManager* window_manager,
    RootWindowController* root_window_controller,
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  // TODO(sky): constrain and validate properties.

  int32_t container_id = kShellWindowId_Invalid;
  aura::Window* context = nullptr;
  aura::Window* container_window = nullptr;
  if (GetInitialContainerId(*properties, &container_id)) {
    container_window = root_window_controller->GetWindow()
                           ->GetChildByShellWindowId(container_id)
                           ->aura_window();
  } else {
    context = root_window_controller->GetRootWindow();
  }

  gfx::Rect bounds = CalculateDefaultBounds(
      window_manager, root_window_controller, container_window, properties);

  const bool provide_non_client_frame =
      window_type == ui::mojom::WindowType::WINDOW ||
      window_type == ui::mojom::WindowType::PANEL;
  if (provide_non_client_frame) {
    // See NonClientFrameController for details on lifetime.
    NonClientFrameController* non_client_frame_controller =
        new NonClientFrameController(container_window, context, bounds,
                                     window_type, properties, window_manager);
    return non_client_frame_controller->window();
  }

  aura::Window* window = new aura::Window(nullptr);
  aura::SetWindowType(window, window_type);
  // Apply properties before Init(), that way they are sent to the server at
  // the time the window is created.
  aura::PropertyConverter* property_converter =
      window_manager->property_converter();
  for (auto& property_pair : *properties) {
    property_converter->SetPropertyFromTransportValue(
        window, property_pair.first, &property_pair.second);
  }
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(bounds);

  if (container_window) {
    container_window->AddChild(window);
  } else {
    WmWindow* root = root_window_controller->GetWindow();
    gfx::Point origin =
        root->ConvertPointToTarget(root->GetRootWindow(), gfx::Point());
    origin += root_window_controller->GetWindow()
                  ->GetDisplayNearestWindow()
                  .bounds()
                  .OffsetFromOrigin();
    gfx::Rect bounds_in_screen(origin, bounds.size());
    ash::wm::GetDefaultParent(WmWindow::Get(context), WmWindow::Get(window),
                              bounds_in_screen)
        ->aura_window()
        ->AddChild(window);
  }
  return window;
}

}  // namespace

aura::Window* CreateAndParentTopLevelWindow(
    WindowManager* window_manager,
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  RootWindowController* root_window_controller =
      GetRootWindowControllerForNewTopLevelWindow(properties);
  aura::Window* window = CreateAndParentTopLevelWindowInRoot(
      window_manager, root_window_controller, window_type, properties);
  DisconnectedAppHandler::Create(window);
  if (properties->count(
          ui::mojom::WindowManager::kWindowIgnoredByShelf_Property)) {
    wm::WindowState* window_state = WmWindow::Get(window)->GetWindowState();
    window_state->set_ignored_by_shelf(mojo::ConvertTo<bool>(
        (*properties)
            [ui::mojom::WindowManager::kWindowIgnoredByShelf_Property]));
    // No need to persist this value.
    properties->erase(ui::mojom::WindowManager::kWindowIgnoredByShelf_Property);
  }
  return window;
}

}  // namespace mus
}  // namespace ash
