// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/bridge/wm_root_window_controller_mus.h"

#include "ash/wm/common/wm_root_window_controller_observer.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mash/wm/bridge/wm_globals_mus.h"
#include "mash/wm/bridge/wm_window_mus.h"
#include "mash/wm/root_window_controller.h"
#include "ui/gfx/display.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"

MUS_DECLARE_WINDOW_PROPERTY_TYPE(mash::wm::WmRootWindowControllerMus*);

namespace mash {
namespace wm {

// TODO(sky): it likely makes more sense to hang this off RootWindowSettings.
MUS_DEFINE_OWNED_WINDOW_PROPERTY_KEY(mash::wm::WmRootWindowControllerMus,
                                     kWmRootWindowControllerKey,
                                     nullptr);

WmRootWindowControllerMus::WmRootWindowControllerMus(
    WmGlobalsMus* globals,
    RootWindowController* root_window_controller)
    : globals_(globals), root_window_controller_(root_window_controller) {
  globals_->AddRootWindowController(this);
  root_window_controller_->root()->SetLocalProperty(kWmRootWindowControllerKey,
                                                    this);
}

WmRootWindowControllerMus::~WmRootWindowControllerMus() {
  globals_->RemoveRootWindowController(this);
}

// static
const WmRootWindowControllerMus* WmRootWindowControllerMus::Get(
    const mus::Window* window) {
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

const gfx::Display& WmRootWindowControllerMus::GetDisplay() const {
  return root_window_controller_->display();
}

bool WmRootWindowControllerMus::HasShelf() {
  NOTIMPLEMENTED();
  return false;
}

ash::wm::WmGlobals* WmRootWindowControllerMus::GetGlobals() {
  return globals_;
}

ash::wm::WorkspaceWindowState
WmRootWindowControllerMus::GetWorkspaceWindowState() {
  NOTIMPLEMENTED();
  return ash::wm::WORKSPACE_WINDOW_STATE_DEFAULT;
}

ash::AlwaysOnTopController*
WmRootWindowControllerMus::GetAlwaysOnTopController() {
  NOTIMPLEMENTED();
  return nullptr;
}

ash::wm::WmShelf* WmRootWindowControllerMus::GetShelf() {
  NOTIMPLEMENTED();
  return nullptr;
}

ash::wm::WmWindow* WmRootWindowControllerMus::GetWindow() {
  return WmWindowMus::Get(root_window_controller_->root());
}

void WmRootWindowControllerMus::ConfigureWidgetInitParamsForContainer(
    views::Widget* widget,
    int shell_container_id,
    views::Widget::InitParams* init_params) {
  init_params->parent_mus =
      root_window_controller_->root()->GetChildByLocalId(shell_container_id);
  DCHECK(init_params->parent_mus);
  mus::Window* new_window =
      root_window_controller_->root()->connection()->NewWindow();
  init_params->native_widget = new views::NativeWidgetMus(
      widget, root_window_controller_->GetConnector(), new_window,
      mus::mojom::SurfaceType::DEFAULT);
}

ash::wm::WmWindow* WmRootWindowControllerMus::FindEventTarget(
    const gfx::Point& location_in_screen) {
  NOTIMPLEMENTED();
  return nullptr;
}

void WmRootWindowControllerMus::AddObserver(
    ash::wm::WmRootWindowControllerObserver* observer) {
  NOTIMPLEMENTED();
}

void WmRootWindowControllerMus::RemoveObserver(
    ash::wm::WmRootWindowControllerObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace wm
}  // namespace mash
