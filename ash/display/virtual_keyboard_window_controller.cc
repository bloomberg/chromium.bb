// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/virtual_keyboard_window_controller.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/root_window_transformers.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

VirtualKeyboardWindowController::VirtualKeyboardWindowController() {
  Shell::GetInstance()->AddShellObserver(this);
}

VirtualKeyboardWindowController::~VirtualKeyboardWindowController() {
  Shell::GetInstance()->RemoveShellObserver(this);
  // Make sure the root window gets deleted before cursor_window_delegate.
  Close();
}

void VirtualKeyboardWindowController::ActivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  root_window_controller_->ActivateKeyboard(keyboard_controller);
}

void VirtualKeyboardWindowController::UpdateWindow(
    const DisplayInfo& display_info) {
  static int virtual_keyboard_host_count = 0;
  if (!root_window_controller_.get()) {
    AshWindowTreeHostInitParams init_params;
    init_params.initial_bounds = display_info.bounds_in_native();
    AshWindowTreeHost* ash_host = AshWindowTreeHost::Create(init_params);
    aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();

    host->window()->SetName(base::StringPrintf("VirtualKeyboardRootWindow-%d",
                                               virtual_keyboard_host_count++));

    // No need to remove WindowTreeHostObserver because the DisplayController
    // outlives the host.
    host->AddObserver(Shell::GetInstance()->display_controller());
    InitRootWindowSettings(host->window())->display_id = display_info.id();
    host->InitHost();
    RootWindowController::CreateForVirtualKeyboardDisplay(ash_host);
    root_window_controller_.reset(GetRootWindowController(host->window()));
    root_window_controller_->GetHost()->Show();
    root_window_controller_->ActivateKeyboard(
        keyboard::KeyboardController::GetInstance());
    FlipDisplay();
  } else {
    aura::WindowTreeHost* host = root_window_controller_->GetHost();
    GetRootWindowSettings(host->window())->display_id = display_info.id();
    host->SetBounds(display_info.bounds_in_native());
  }
}

void VirtualKeyboardWindowController::Close() {
  if (root_window_controller_.get()) {
    root_window_controller_->GetHost()->RemoveObserver(
        Shell::GetInstance()->display_controller());
    root_window_controller_->Shutdown();
    root_window_controller_.reset();
  }
}

void VirtualKeyboardWindowController::FlipDisplay() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->virtual_keyboard_root_window_enabled()) {
    NOTREACHED() << "Attempting to flip virtual keyboard root window when it "
                 << "is not enabled.";
    return;
  }
  display_manager->SetDisplayRotation(
      display_manager->non_desktop_display().id(), gfx::Display::ROTATE_180);

  aura::WindowTreeHost* host = root_window_controller_->GetHost();
  scoped_ptr<RootWindowTransformer> transformer(
      CreateRootWindowTransformerForDisplay(
          host->window(), display_manager->non_desktop_display()));
  root_window_controller_->ash_host()->SetRootWindowTransformer(
      transformer.Pass());
}

void VirtualKeyboardWindowController::OnMaximizeModeStarted() {
  keyboard::SetTouchKeyboardEnabled(true);
  Shell::GetInstance()->CreateKeyboard();
}

void VirtualKeyboardWindowController::OnMaximizeModeEnded() {
  keyboard::SetTouchKeyboardEnabled(false);
  if (!keyboard::IsKeyboardEnabled())
    Shell::GetInstance()->DeactivateKeyboard();
}

}  // namespace ash
