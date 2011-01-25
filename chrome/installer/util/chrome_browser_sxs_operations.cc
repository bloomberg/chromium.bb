// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_browser_sxs_operations.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeBrowserSxSOperations::AppendProductFlags(
    const std::set<std::wstring>& options,
    CommandLine* uninstall_command) const {
  DCHECK(uninstall_command);

  uninstall_command->AppendSwitch(switches::kChromeSxS);
  ChromeBrowserOperations::AppendProductFlags(options, uninstall_command);
}

}  // namespace installer
