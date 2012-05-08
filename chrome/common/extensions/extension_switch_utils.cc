// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_switch_utils.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

namespace switch_utils {

bool IsOffStoreInstallEnabled() {
  // Disabling off-store installation is not yet implemented for Aura. Not sure
  // what the Aura equivalent for this UI is.
#if defined(USE_AURA)
  return true;
#else
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableOffStoreExtensionInstall);
#endif
}

}  // switch_utils

}  // extensions
