// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen.h"

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_debug_view.h"
#include "ash/login/ui/lock_window.h"
#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chromeos/chromeos_switches.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

views::View* BuildContentsView(LoginDataDispatcher* data_dispatcher) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kShowLoginDevOverlay)) {
    return new LockDebugView(data_dispatcher);
  }
  return new LockContentsView(data_dispatcher);
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

LockScreen::LockScreen() = default;

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

  auto data_dispatcher = base::MakeUnique<LoginDataDispatcher>();
  auto* contents = BuildContentsView(data_dispatcher.get());

  // TODO(jdufault|crbug.com/731191): Call NotifyUsers via
  // LockScreenController::LoadUsers once it uses a mojom specific type.
  std::vector<mojom::UserInfoPtr> users;
  for (const mojom::UserSessionPtr& session :
       Shell::Get()->session_controller()->GetUserSessions()) {
    users.push_back(session->user_info->Clone());
  }
  data_dispatcher->NotifyUsers(users);

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

void LockScreen::SetPinEnabledForUser(const AccountId& account_id,
                                      bool is_enabled) {
  window_->data_dispatcher()->SetPinEnabledForUser(account_id, is_enabled);
}

}  // namespace ash
