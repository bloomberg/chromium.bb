// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/primary_display_switch_observer.h"

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
  StorePrimaryDisplayIDPref(gfx::Screen::GetPrimaryDisplay().id());
}

PrimaryDisplaySwitchObserver::PrimaryDisplaySwitchObserver()
    : primary_root_(ash::Shell::GetInstance()->GetPrimaryRootWindow()) {
  primary_root_->AddRootWindowObserver(this);
}

PrimaryDisplaySwitchObserver::~PrimaryDisplaySwitchObserver() {
  primary_root_->RemoveRootWindowObserver(this);
}

}  // namespace chromeos
