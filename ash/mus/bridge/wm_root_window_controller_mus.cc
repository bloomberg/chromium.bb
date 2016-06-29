// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_root_window_controller_mus.h"

#include "ash/common/wm/workspace/workspace_layout_manager.h"
#include "ash/common/wm/workspace/workspace_layout_manager_backdrop_delegate.h"
#include "ash/common/wm_root_window_controller_observer.h"
#include "ash/mus/bridge/wm_shelf_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/container_ids.h"
#include "ash/mus/root_window_controller.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_client.h"
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
    : shell_(shell), root_window_controller_(root_window_controller) {
  shell_->AddRootWindowController(this);
  root_window_controller_->root()->SetLocalProperty(kWmRootWindowControllerKey,
                                                    this);
}

WmRootWindowControllerMus::~WmRootWindowControllerMus() {
  shell_->RemoveRootWindowController(this);
}

// static
const WmRootWindowControllerMus* WmRootWindowControllerMus::Get(
    const ::mus::Window* window) {
  if (!window)
    return nullptr;

  return window->GetRoot()->GetLocalProperty(kWmRootWindowControllerKey);
}

void WmRootWindowControllerMus::NotifyFullscreenStateChange(
    bool is_fullscreen) {
  FOR_EACH_OBSERVER(WmRootWindowControllerObserver, observers_,
                    OnFullscreenStateChanged(is_fullscreen));
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

bool WmRootWindowControllerMus::HasShelf() {
  return GetShelf() != nullptr;
}

WmShell* WmRootWindowControllerMus::GetShell() {
  return shell_;
}

wm::WorkspaceWindowState WmRootWindowControllerMus::GetWorkspaceWindowState() {
  NOTIMPLEMENTED();
  return wm::WORKSPACE_WINDOW_STATE_DEFAULT;
}

void WmRootWindowControllerMus::SetMaximizeBackdropDelegate(
    std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate) {
  root_window_controller_->workspace_layout_manager()
      ->SetMaximizeBackdropDelegate(std::move(delegate));
}

AlwaysOnTopController* WmRootWindowControllerMus::GetAlwaysOnTopController() {
  return root_window_controller_->always_on_top_controller();
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
  ::mus::Window* new_window =
      root_window_controller_->root()->window_tree()->NewWindow();
  WmWindowMus::Get(new_window)
      ->set_widget(widget, WmWindowMus::WidgetCreationType::INTERNAL);
  init_params->native_widget = new views::NativeWidgetMus(
      widget, root_window_controller_->GetConnector(), new_window,
      ::mus::mojom::SurfaceType::DEFAULT);
}

WmWindow* WmRootWindowControllerMus::FindEventTarget(
    const gfx::Point& location_in_screen) {
  NOTIMPLEMENTED();
  return nullptr;
}

void WmRootWindowControllerMus::AddObserver(
    WmRootWindowControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void WmRootWindowControllerMus::RemoveObserver(
    WmRootWindowControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace mus
}  // namespace ash
