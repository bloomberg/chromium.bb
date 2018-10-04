// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_helper.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace content {

std::unique_ptr<base::FieldTrialList> SetUpFieldTrialsAndFeatureList() {
  auto field_trial_list = std::make_unique<base::FieldTrialList>(nullptr);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Ensure any field trials specified on the command line are initialized.
  if (command_line->HasSwitch(::switches::kForceFieldTrials)) {
    // Create field trials without activating them, so that this behaves in a
    // consistent manner with field trials created from the server.
    bool result = base::FieldTrialList::CreateTrialsFromString(
        command_line->GetSwitchValueASCII(::switches::kForceFieldTrials),
        std::set<std::string>());
    CHECK(result) << "Invalid --" << ::switches::kForceFieldTrials
                  << " list specified.";
  }

  base::FeatureList::InitializeInstance(
      command_line->GetSwitchValueASCII(switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableFeatures));
  return field_trial_list;
}

}  // namespace content
