// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ash_config.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "services/service_manager/runner/common/client_util.h"

namespace chromeos {

namespace {

ash::Config ComputeAshConfig() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  ash::Config config = ash::Config::CLASSIC;
  if (command_line->HasSwitch(switches::kMash))
    config = ash::Config::MASH;
  else if (command_line->HasSwitch(switches::kMus))
    config = ash::Config::MUS;
  VLOG_IF(1, config != ash::Config::CLASSIC &&
                 !service_manager::ServiceManagerIsRemote())
      << " Running with a simulated ash config (likely for testing).";
  return config;
}

}  // namespace

ash::Config GetAshConfig() {
  static const ash::Config config = ComputeAshConfig();
  return config;
}

}  // namespace chromeos
