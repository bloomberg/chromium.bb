// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/aura/wm_root_window_controller_aura.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/aura/wm_globals_aura.h"
#include "ash/wm/aura/wm_shelf_aura.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/wm_root_window_controller_observer.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::wm::WmRootWindowControllerAura*);

namespace ash {
namespace wm {

// TODO(sky): it likely makes more sense to hang this off RootWindowSettings.
DEFINE_OWNED_WINDOW_PROPERTY_KEY(ash::wm::WmRootWindowControllerAura,
                                 kWmRootWindowControllerKey,
                                 nullptr);

// static
WmRootWindowController* WmRootWindowController::GetWithDisplayId(int64_t id) {
  return WmRootWindowControllerAura::Get(Shell::GetInstance()
                                             ->window_tree_host_manager()
                                             ->GetRootWindowForDisplayId(id));
}

WmRootWindowControllerAura::WmRootWindowControllerAura(
    RootWindowController* root_window_controller)
    : root_window_controller_(root_window_controller) {
  root_window_controller_->GetRootWindow()->SetProperty(
      kWmRootWindowControllerKey, this);
  Shell::GetInstance()->AddShellObserver(this);
}

WmRootWindowControllerAura::~WmRootWindowControllerAura() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

// static
const WmRootWindowControllerAura* WmRootWindowControllerAura::Get(
    const aura::Window* window) {
  if (!window)
    return nullptr;

  RootWindowController* root_window_controller =
      GetRootWindowController(window);
  if (!root_window_controller)
    return nullptr;

  WmRootWindowControllerAura* wm_root_window_controller =
      root_window_controller->GetRootWindow()->GetProperty(
          kWmRootWindowControllerKey);
  if (wm_root_window_controller)
    return wm_root_window_controller;

  // WmRootWindowControllerAura is owned by the RootWindowController's window.
  return new WmRootWindowControllerAura(root_window_controller);
}

bool WmRootWindowControllerAura::HasShelf() {
  return root_window_controller_->shelf() != nullptr;
}

WmGlobals* WmRootWindowControllerAura::GetGlobals() {
  return WmGlobalsAura::Get();
}

WorkspaceWindowState WmRootWindowControllerAura::GetWorkspaceWindowState() {
  return root_window_controller_->workspace_controller()->GetWindowState();
}

WmShelf* WmRootWindowControllerAura::GetShelf() {
  return root_window_controller_->shelf()
             ? root_window_controller_->shelf()->shelf()->wm_shelf()
             : nullptr;
}

WmWindow* WmRootWindowControllerAura::GetWindow() {
  return WmWindowAura::Get(root_window_controller_->GetRootWindow());
}

void WmRootWindowControllerAura::ConfigureWidgetInitParamsForContainer(
    views::Widget* widget,
    int shell_container_id,
    views::Widget::InitParams* init_params) {
  init_params->parent = Shell::GetContainer(
      root_window_controller_->GetRootWindow(), shell_container_id);
}

void WmRootWindowControllerAura::AddObserver(
    WmRootWindowControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void WmRootWindowControllerAura::RemoveObserver(
    WmRootWindowControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WmRootWindowControllerAura::OnDisplayWorkAreaInsetsChanged() {
  FOR_EACH_OBSERVER(WmRootWindowControllerObserver, observers_,
                    OnWorkAreaChanged());
}

void WmRootWindowControllerAura::OnFullscreenStateChanged(
    bool is_fullscreen,
    aura::Window* root_window) {
  if (root_window != root_window_controller_->GetRootWindow())
    return;

  FOR_EACH_OBSERVER(WmRootWindowControllerObserver, observers_,
                    OnFullscreenStateChanged(is_fullscreen));
}

void WmRootWindowControllerAura::OnShelfAlignmentChanged(
    aura::Window* root_window) {
  if (root_window != root_window_controller_->GetRootWindow())
    return;

  FOR_EACH_OBSERVER(WmRootWindowControllerObserver, observers_,
                    OnShelfAlignmentChanged());
}

}  // namespace wm
}  // namespace ash
