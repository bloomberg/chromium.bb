// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/aura/wm_root_window_controller_aura.h"

#include "ash/root_window_controller.h"
#include "ash/wm/aura/wm_globals_aura.h"
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

WmRootWindowControllerAura::WmRootWindowControllerAura(
    RootWindowController* root_window_controller)
    : root_window_controller_(root_window_controller) {
  root_window_controller_->GetRootWindow()->SetProperty(
      kWmRootWindowControllerKey, this);
}

WmRootWindowControllerAura::~WmRootWindowControllerAura() {}

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

}  // namespace wm
}  // namespace ash
