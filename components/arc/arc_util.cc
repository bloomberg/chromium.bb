// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_util.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user_manager.h"

namespace arc {

namespace {

// This is for finch. See also crbug.com/633704 for details.
// TODO(hidehiko): More comments of the intention how this works, when
// we unify the commandline flags.
const base::Feature kEnableArcFeature{"EnableARC",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

bool IsArcAvailable() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  // TODO(hidehiko): Unify --enable-arc and --arc-available flags.
  // If switches::kEnableArc is set, the device is officially supported to run
  // ARC. If it is not, but switches::kArcAvailable is set, ARC is installed
  // but is not allowed to run unless |kEnableArcFeature| is true.
  return command_line->HasSwitch(chromeos::switches::kEnableArc) ||
         (command_line->HasSwitch(chromeos::switches::kArcAvailable) &&
          base::FeatureList::IsEnabled(kEnableArcFeature));
}

void SetArcAvailableCommandLineForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kEnableArc);
}

bool IsArcKioskMode() {
  return user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp();
}

bool IsArcOptInVerificationDisabled() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
      chromeos::switches::kDisableArcOptInVerification);
}

}  // namespace arc
