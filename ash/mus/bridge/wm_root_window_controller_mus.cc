// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_root_window_controller_mus.h"

#include "ash/mus/bridge/wm_shelf_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/root_window_controller.h"
#include "ash/mus/window_manager.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::mus::WmRootWindowControllerMus*);

namespace {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(ash::mus::WmRootWindowControllerMus*,
                                 kWmRootWindowControllerKey,
                                 nullptr);

}  // namespace

namespace ash {
namespace mus {

WmRootWindowControllerMus::WmRootWindowControllerMus(
    WmShellMus* shell,
    RootWindowController* root_window_controller)
    : WmRootWindowController(WmWindowMus::Get(root_window_controller->root())),
      shell_(shell),
      root_window_controller_(root_window_controller) {
  shell_->AddRootWindowController(this);
  root_window_controller_->root()->SetProperty(kWmRootWindowControllerKey,
                                               this);
}

WmRootWindowControllerMus::~WmRootWindowControllerMus() {
  shell_->RemoveRootWindowController(this);
}

// static
const WmRootWindowControllerMus* WmRootWindowControllerMus::Get(
    const aura::Window* window) {
  if (!window)
    return nullptr;

  return window->GetRootWindow()->GetProperty(kWmRootWindowControllerKey);
}

gfx::Point WmRootWindowControllerMus::ConvertPointToScreen(
    const WmWindowMus* source,
    const gfx::Point& point) const {
  gfx::Point point_in_root =
      source->ConvertPointToTarget(source->GetRootWindow(), point);
  point_in_root += GetDisplay().bounds().OffsetFromOrigin();
  return point_in_root;
}

const display::Display& WmRootWindowControllerMus::GetDisplay() const {
  return root_window_controller_->display();
}

void WmRootWindowControllerMus::MoveWindowsTo(WmWindow* dest) {
  WmRootWindowController::MoveWindowsTo(dest);
}

bool WmRootWindowControllerMus::HasShelf() {
  return GetShelf() != nullptr;
}

WmShell* WmRootWindowControllerMus::GetShell() {
  return shell_;
}

WmShelf* WmRootWindowControllerMus::GetShelf() {
  return root_window_controller_->wm_shelf();
}

WmWindow* WmRootWindowControllerMus::GetWindow() {
  return WmWindowMus::Get(root_window_controller_->root());
}

void WmRootWindowControllerMus::ConfigureWidgetInitParamsForContainer(
    views::Widget* widget,
    int shell_container_id,
    views::Widget::InitParams* init_params) {
  init_params->parent = WmWindowMus::GetAuraWindow(
      WmWindowMus::Get(root_window_controller_->root())
          ->GetChildByShellWindowId(shell_container_id));
  DCHECK(init_params->parent);
}

WmWindow* WmRootWindowControllerMus::FindEventTarget(
    const gfx::Point& location_in_screen) {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::Point WmRootWindowControllerMus::GetLastMouseLocationInRoot() {
  gfx::Point location = root_window_controller_->window_manager()
                            ->window_tree_client()
                            ->GetCursorScreenPoint();
  location -=
      root_window_controller_->display().bounds().origin().OffsetFromOrigin();
  return location;
}

bool WmRootWindowControllerMus::ShouldDestroyWindowInCloseChildWindows(
    WmWindow* window) {
  aura::WindowTreeClient* window_tree_client =
      root_window_controller_->window_manager()->window_tree_client();
  aura::Window* aura_window = WmWindowMus::GetAuraWindow(window);
  aura::WindowMus* window_mus = aura::WindowMus::Get(aura_window);
  return window_tree_client->WasCreatedByThisClient(window_mus) ||
         window_tree_client->IsRoot(window_mus);
}

}  // namespace mus
}  // namespace ash
