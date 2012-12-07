// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/primary_display_switch_observer.h"

#include "ash/display/display_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace chromeos {

void PrimaryDisplaySwitchObserver::OnRootWindowMoved(
    const aura::RootWindow* root_window, const gfx::Point& new_origin) {
  StorePrimaryDisplayIDPref(ash::Shell::GetScreen()->GetPrimaryDisplay().id());

  if (gfx::Screen::GetNativeScreen()->GetNumDisplays() < 2)
    return;

  const gfx::Display& display = ash::ScreenAsh::GetSecondaryDisplay();
  if (!display.is_valid())
    return;

  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  ash::DisplayLayout layout = display_controller->GetLayoutForDisplay(display);
  SetDisplayLayoutPref(
      display, static_cast<int>(layout.position), layout.offset);
}

PrimaryDisplaySwitchObserver::PrimaryDisplaySwitchObserver()
    : primary_root_(ash::Shell::GetInstance()->GetPrimaryRootWindow()) {
  primary_root_->AddRootWindowObserver(this);
}

PrimaryDisplaySwitchObserver::~PrimaryDisplaySwitchObserver() {
  primary_root_->RemoveRootWindowObserver(this);
}

}  // namespace chromeos
