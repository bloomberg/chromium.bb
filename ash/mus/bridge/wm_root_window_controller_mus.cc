// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_root_window_controller_mus.h"

#include "ash/mus/bridge/wm_shelf_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/container_ids.h"
#include "ash/mus/root_window_controller.h"
#include "ash/mus/window_manager.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/display/display.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"

MUS_DECLARE_WINDOW_PROPERTY_TYPE(ash::mus::WmRootWindowControllerMus*);

namespace {

MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(ash::mus::WmRootWindowControllerMus*,
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
  root_window_controller_->root()->SetLocalProperty(kWmRootWindowControllerKey,
                                                    this);
}

WmRootWindowControllerMus::~WmRootWindowControllerMus() {
  shell_->RemoveRootWindowController(this);
}

// static
const WmRootWindowControllerMus* WmRootWindowControllerMus::Get(
    const ui::Window* window) {
  if (!window)
    return nullptr;

  return window->GetRoot()->GetLocalProperty(kWmRootWindowControllerKey);
}

gfx::Point WmRootWindowControllerMus::ConvertPointToScreen(
    const WmWindowMus* source,
    const gfx::Point& point) const {
  gfx::Point point_in_root =
      source->ConvertPointToTarget(source->GetRootWindow(), point);
  point_in_root += GetDisplay().bounds().OffsetFromOrigin();
  return point_in_root;
}

gfx::Point WmRootWindowControllerMus::ConvertPointFromScreen(
    const WmWindowMus* target,
    const gfx::Point& point) const {
  gfx::Point result = point;
  result -= GetDisplay().bounds().OffsetFromOrigin();
  return target->GetRootWindow()->ConvertPointToTarget(target, result);
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
  init_params->parent_mus = WmWindowMus::GetMusWindow(
      WmWindowMus::Get(root_window_controller_->root())
          ->GetChildByShellWindowId(shell_container_id));
  DCHECK(init_params->parent_mus);
  ui::Window* new_window =
      root_window_controller_->root()->window_tree()->NewWindow(
          &(init_params->mus_properties));
  WmWindowMus::Get(new_window)
      ->set_widget(widget, WmWindowMus::WidgetCreationType::INTERNAL);
  init_params->native_widget = new views::NativeWidgetMus(
      widget, new_window, ui::mojom::SurfaceType::DEFAULT);
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
  ui::Window* ui_window = WmWindowMus::GetMusWindow(window);
  return ui_window->WasCreatedByThisClient() ||
         ui_window->window_tree()->GetRoots().count(ui_window);
}

}  // namespace mus
}  // namespace ash
