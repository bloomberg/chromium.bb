// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_configuration_observer.h"

#include "ash/display/display_prefs.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

DisplayConfigurationObserver::DisplayConfigurationObserver() {
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
}

DisplayConfigurationObserver::~DisplayConfigurationObserver() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
}

void DisplayConfigurationObserver::OnDisplaysInitialized() {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  // Update the display pref with the initial power state.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kFirstExecAfterBoot) &&
      save_preference_) {
    Shell::Get()->display_prefs()->StoreDisplayPrefs();
  }
}

void DisplayConfigurationObserver::OnDisplayConfigurationChanged() {
  if (save_preference_)
    Shell::Get()->display_prefs()->StoreDisplayPrefs();
}

void DisplayConfigurationObserver::OnTabletModeStarted() {
  // Setting mirror mode may destroy the secondary SystemTray, so use
  // PostTask to set mirror mode in the next frame in case other
  // TabletModeObserver entries are owned by the SystemTray.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DisplayConfigurationObserver::StartMirrorMode,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DisplayConfigurationObserver::OnTabletModeEnded() {
  // See comment for OnTabletModeStarted.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DisplayConfigurationObserver::EndMirrorMode,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DisplayConfigurationObserver::StartMirrorMode() {
  // TODO(oshima): Tablet mode defaults to mirror mode until we figure out
  // how to handle this scenario, and we shouldn't save this state.
  // https://crbug.com/733092.
  save_preference_ = false;
  // TODO(oshima): Mirroring won't work with 3+ displays:
  // https://crbug.com/737667.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  was_in_mirror_mode_ = display_manager->IsInMirrorMode();
  display_manager->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  display_manager->layout_store()->set_forced_mirror_mode_for_tablet(true);
}

void DisplayConfigurationObserver::EndMirrorMode() {
  if (!was_in_mirror_mode_) {
    Shell::Get()->display_manager()->SetMirrorMode(display::MirrorMode::kOff,
                                                   base::nullopt);
  }
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->layout_store()->set_forced_mirror_mode_for_tablet(false);
  save_preference_ = true;
}

}  // namespace ash
