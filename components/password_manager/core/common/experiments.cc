// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/experiments.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/password_manager/core/common/password_manager_switches.h"

namespace password_manager {

bool ManageAccountLinkExperimentEnabled() {
  std::string group_name =
      base::FieldTrialList::FindFullName("PasswordLinkInSettings");

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisablePasswordLink))
    return false;

  if (command_line->HasSwitch(switches::kEnablePasswordLink))
    return true;

  return group_name == "Enabled";
}

}  // namespace password_manager
