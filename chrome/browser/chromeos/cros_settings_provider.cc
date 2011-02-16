// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings_provider.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

namespace chromeos {

void CrosSettingsProvider::Set(const std::string& path, Value* value) {
  // We don't allow changing any of the cros settings without prefix
  // "cros.session." in the guest mode.
  // It should not reach here from UI in the guest mode, but just in case.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession) &&
      !::StartsWithASCII(path, "cros.session.", true)) {
    LOG(ERROR) << "Ignoring the guest request to change: " << path;
    return;
  }
  DoSet(path, value);
}

};  // namespace chromeos
