// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen.h"

#include <memory>
#include <utility>

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_debug_view.h"
#include "ash/login/ui/lock_window.h"
#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

views::View* BuildContentsView(mojom::TrayActionState initial_note_action_state,
                               LoginDataDispatcher* data_dispatcher) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kShowLoginDevOverlay)) {
    return new LockDebugView(initial_note_action_state, data_dispatcher);
  }
  return new LockContentsView(initial_note_action_state, data_dispatcher);
}

ui::Layer* GetWallpaperLayerForWindow(aura::Window* window) {
  return RootWindowController::ForWindow(window)
      ->wallpaper_widget_controller()
      ->widget()
      ->GetLayer();
}

// Global lock screen instance. There can only ever be on lock screen at a
// time.
LockScreen* instance_ = nullptr;

}  // namespace

LockScreen::LockScreen()
    : tray_action_observer_(this), session_observer_(this) {
  tray_action_observer_.Add(ash::Shell::Get()->tray_action());
}

LockScreen::~LockScreen() = default;

// static
LockScreen* LockScreen::Get() {
  CHECK(instance_);
  return instance_;
}

// static
void LockScreen::Show() {
  CHECK(!instance_);
  instance_ = new LockScreen();

  auto data_dispatcher = std::make_unique<LoginDataDispatcher>();
  auto* contents = BuildContentsView(
      ash::Shell::Get()->tray_action()->GetLockScreenNoteState(),
      data_dispatcher.get());

  auto* window = instance_->window_ = new LockWindow(Shell::GetAshConfig());
  window->SetBounds(display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  window->SetContentsView(contents);
  window->set_data_dispatcher(std::move(data_dispatcher));
  window->Show();
}

// static
bool LockScreen::IsShown() {
  return !!instance_;
}

void LockScreen::Destroy() {
  CHECK_EQ(instance_, this);

  // Restore the initial wallpaper bluriness if they were changed.
  for (auto it = initial_blur_.begin(); it != initial_blur_.end(); ++it)
    it->first->SetLayerBlur(it->second);
  window_->Close();
  delete instance_;
  instance_ = nullptr;
}

void LockScreen::ToggleBlurForDebug() {
  // Save the initial wallpaper bluriness upon the first time this is called.
  if (instance_->initial_blur_.empty()) {
    for (aura::Window* window : Shell::GetAllRootWindows()) {
      ui::Layer* layer = GetWallpaperLayerForWindow(window);
      instance_->initial_blur_[layer] = layer->layer_blur();
    }
  }
  for (aura::Window* window : Shell::GetAllRootWindows()) {
    ui::Layer* layer = GetWallpaperLayerForWindow(window);
    if (layer->layer_blur() > 0.0f) {
      layer->SetLayerBlur(0.0f);
    } else {
      layer->SetLayerBlur(login_constants::kBlurSigma);
    }
  }
}

LoginDataDispatcher* LockScreen::data_dispatcher() {
  return window_->data_dispatcher();
}

void LockScreen::OnLockScreenNoteStateChanged(mojom::TrayActionState state) {
  if (data_dispatcher())
    data_dispatcher()->SetLockScreenNoteState(state);
}

void LockScreen::OnLockStateChanged(bool locked) {
  if (!locked)
    Destroy();
}

}  // namespace ash
