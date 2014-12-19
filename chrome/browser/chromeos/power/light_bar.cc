// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/light_bar.h"

#include <cstring>  // needed for strlen()

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {
const int kDarkSuspendDelaySeconds = 4;
const char kLightBarControlPath[] =
    "/sys/devices/platform/cros_ec_lpc.0/cros-ec-dev.0/chromeos/cros_ec/"
    "lightbar/sequence";
const char kKonamiCommand[] = "KONAMI";

}  // namespace

LightBar::LightBar()
    : control_path_(base::FilePath(kLightBarControlPath)),
      has_light_bar_(base::PathIsWritable(control_path_)) {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

LightBar::~LightBar() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void LightBar::DarkSuspendImminent() {
  // The plan is to track the progress of push events and then re-suspend as
  // soon as we know they have been processed.  In the meanwhile, we can hack
  // around this by just having the light bar delay for a long time so that
  // people can at least start playing around with this feature for their apps.
  //
  // TODO(chirantan): Remove this once we can accurately track the progress of
  // push events.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      DBusThreadManager::Get()
          ->GetPowerManagerClient()
          ->GetSuspendReadinessCallback(),
      base::TimeDelta::FromSeconds(kDarkSuspendDelaySeconds));

  if (!has_light_bar_)
    return;

  if (base::WriteFile(control_path_, kKonamiCommand, strlen(kKonamiCommand)) !=
      static_cast<int>(strlen(kKonamiCommand)))
    PLOG(WARNING) << "Unable to flash light bar during dark resume.";
}

}  // namespace chromeos
