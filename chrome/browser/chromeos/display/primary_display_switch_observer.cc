// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/primary_display_switch_observer.h"

#include "ash/shell.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "ui/aura/root_window.h"

namespace chromeos {

PrimaryDisplaySwitchObserver::PrimaryDisplaySwitchObserver()
    : primary_root_(ash::Shell::GetInstance()->GetPrimaryRootWindow()) {
  primary_root_->AddRootWindowObserver(this);
  primary_root_->AddObserver(this);
}

PrimaryDisplaySwitchObserver::~PrimaryDisplaySwitchObserver() {
  primary_root_->RemoveRootWindowObserver(this);
  primary_root_->RemoveObserver(this);
}

void PrimaryDisplaySwitchObserver::OnRootWindowMoved(
    const aura::RootWindow* root_window, const gfx::Point& new_origin) {
  StoreDisplayPrefs();
}

void PrimaryDisplaySwitchObserver::OnWindowDestroying(aura::Window* window) {
  if (primary_root_ == window) {
    primary_root_->RemoveRootWindowObserver(this);
    primary_root_->RemoveObserver(this);
    primary_root_ = ash::Shell::GetInstance()->GetPrimaryRootWindow();
    primary_root_->AddRootWindowObserver(this);
    primary_root_->AddObserver(this);
  }
}

}  // namespace chromeos
