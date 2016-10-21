// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_configuration_observer.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chromeos/chromeos_switches.h"

namespace chromeos {

DisplayConfigurationObserver::DisplayConfigurationObserver() {
  ash::Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
}

DisplayConfigurationObserver::~DisplayConfigurationObserver() {
  ash::Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);
}

void DisplayConfigurationObserver::OnDisplaysInitialized() {
  // Update the display pref with the initial power state.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kFirstExecAfterBoot))
    StoreDisplayPrefs();
}

void DisplayConfigurationObserver::OnDisplayConfigurationChanged() {
  StoreDisplayPrefs();
}

}  // namespace chromeos
