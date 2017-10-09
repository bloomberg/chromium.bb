// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_configuration_observer.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chromeos/chromeos_switches.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"

namespace chromeos {

DisplayConfigurationObserver::DisplayConfigurationObserver() {
  ash::Shell::Get()->window_tree_host_manager()->AddObserver(this);
}

DisplayConfigurationObserver::~DisplayConfigurationObserver() {
  ash::Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  ash::Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
}

void DisplayConfigurationObserver::OnDisplaysInitialized() {
  ash::Shell::Get()->tablet_mode_controller()->AddObserver(this);
  // Update the display pref with the initial power state.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kFirstExecAfterBoot) &&
      save_preference_) {
    StoreDisplayPrefs();
  }
}

void DisplayConfigurationObserver::OnDisplayConfigurationChanged() {
  if (save_preference_)
    StoreDisplayPrefs();
}

void DisplayConfigurationObserver::OnTabletModeStarted() {
  // TODO(oshima): Tablet mode defaults to mirror mode until we figure out
  // how to handle this scenario, and we shouldn't save this state.
  // crbug.com/733092.
  save_preference_ = false;
  // TODO(oshima): Mirroring won't work with 3 displays. crbug.com/737667.
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  was_in_mirror_mode_ = display_manager->IsInMirrorMode();
  display_manager->SetMirrorMode(true);
  display_manager->layout_store()->set_forced_mirror_mode(true);
}

void DisplayConfigurationObserver::OnTabletModeEnded() {
  if (!was_in_mirror_mode_)
    ash::Shell::Get()->display_manager()->SetMirrorMode(false);
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  display_manager->layout_store()->set_forced_mirror_mode(false);
  save_preference_ = true;
}

}  // namespace chromeos
