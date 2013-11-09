// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/virtual_keyboard_window_controller.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"

namespace ash {
namespace internal {

VirtualKeyboardWindowController::VirtualKeyboardWindowController() {
}

VirtualKeyboardWindowController::~VirtualKeyboardWindowController() {
  // Make sure the root window gets deleted before cursor_window_delegate.
  Close();
}

void VirtualKeyboardWindowController::UpdateWindow(
    const DisplayInfo& display_info) {
  static int virtual_keyboard_root_window_count = 0;
  if (!root_window_controller_.get()) {
    const gfx::Rect& bounds_in_native = display_info.bounds_in_native();
    aura::RootWindow::CreateParams params(bounds_in_native);
    params.host = Shell::GetInstance()->root_window_host_factory()->
        CreateRootWindowHost(bounds_in_native);
    aura::RootWindow* root_window = new aura::RootWindow(params);

    root_window->window()->SetName(
        base::StringPrintf("VirtualKeyboardRootWindow-%d",
                           virtual_keyboard_root_window_count++));

    // No need to remove RootWindowObserver because
    // the DisplayController object outlives RootWindow objects.
    root_window->AddRootWindowObserver(
        Shell::GetInstance()->display_controller());
    InitRootWindowSettings(root_window->window())->display_id =
        display_info.id();
    root_window->Init();
    RootWindowController::CreateForVirtualKeyboardDisplay(root_window);
    root_window_controller_.reset(GetRootWindowController(
        root_window->window()));
    root_window_controller_->dispatcher()->ShowRootWindow();
  } else {
    aura::RootWindow* root_window = root_window_controller_->dispatcher();
    GetRootWindowSettings(root_window->window())->display_id =
        display_info.id();
    root_window->SetHostBounds(display_info.bounds_in_native());
  }
}

void VirtualKeyboardWindowController::Close() {
  if (root_window_controller_.get()) {
    root_window_controller_->dispatcher()->RemoveRootWindowObserver(
        Shell::GetInstance()->display_controller());
    root_window_controller_->Shutdown();
    root_window_controller_.reset();
  }
}

}  // namespace internal
}  // namespace ash
