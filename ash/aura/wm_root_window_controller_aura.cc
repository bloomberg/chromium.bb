// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_root_window_controller_aura.h"

#include "ash/aura/wm_shell_aura.h"
#include "ash/aura/wm_window_aura.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::WmRootWindowControllerAura*);

namespace ash {

// TODO(sky): it likely makes more sense to hang this off RootWindowSettings.
DEFINE_OWNED_WINDOW_PROPERTY_KEY(ash::WmRootWindowControllerAura,
                                 kWmRootWindowControllerKey,
                                 nullptr);

WmRootWindowControllerAura::WmRootWindowControllerAura(
    RootWindowController* root_window_controller)
    : WmRootWindowController(
          WmWindowAura::Get(root_window_controller->GetRootWindow())),
      root_window_controller_(root_window_controller) {
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

  DCHECK_EQ(aura::Env::Mode::LOCAL, aura::Env::GetInstance()->mode());
  // WmRootWindowControllerAura is owned by the RootWindowController's window.
  return new WmRootWindowControllerAura(root_window_controller);
}

bool WmRootWindowControllerAura::HasShelf() {
  return root_window_controller_->wm_shelf()->shelf_widget() != nullptr;
}

WmShell* WmRootWindowControllerAura::GetShell() {
  return WmShell::Get();
}

WmShelf* WmRootWindowControllerAura::GetShelf() {
  return root_window_controller_->wm_shelf();
}

WmWindow* WmRootWindowControllerAura::GetWindow() {
  return WmWindowAura::Get(root_window_controller_->GetRootWindow());
}

void WmRootWindowControllerAura::OnInitialWallpaperAnimationStarted() {
  root_window_controller_->OnInitialWallpaperAnimationStarted();
  WmRootWindowController::OnInitialWallpaperAnimationStarted();
}

void WmRootWindowControllerAura::OnWallpaperAnimationFinished(
    views::Widget* widget) {
  root_window_controller_->OnWallpaperAnimationFinished(widget);
  WmRootWindowController::OnWallpaperAnimationFinished(widget);
}

bool WmRootWindowControllerAura::ShouldDestroyWindowInCloseChildWindows(
    WmWindow* window) {
  return WmWindowAura::GetAuraWindow(window)->owned_by_parent();
}

}  // namespace ash
