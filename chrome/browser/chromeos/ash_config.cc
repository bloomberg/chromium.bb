// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ash_config.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "services/service_manager/runner/common/client_util.h"

namespace chromeos {

ash::Config GetAshConfig() {
  if (!service_manager::ServiceManagerIsRemote())
    return ash::Config::CLASSIC;

  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kMusConfig) == switches::kMash
             ? ash::Config::MASH
             : ash::Config::MUS;
}

}  // namespace chromeos
