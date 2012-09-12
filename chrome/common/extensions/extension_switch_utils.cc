// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_switch_utils.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

namespace switch_utils {

bool IsEasyOffStoreInstallEnabled() {
  // Disabling easy off-store installation is not yet implemented for Aura. Not
  // sure what the Aura equivalent for this UI is.
#if defined(USE_AURA)
  return true;
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableEasyOffStoreExtensionInstall);
#endif
}

bool IsActionBoxEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableActionBox);
}

bool IsExtensionsInActionBoxEnabled() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableActionBox) &&
         command_line->HasSwitch(switches::kEnableExtensionsInActionBox);
}

bool AreScriptBadgesEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableScriptBadges);
}

}  // switch_utils

}  // extensions
