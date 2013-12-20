// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_browser_sxs_operations.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeBrowserSxSOperations::AppendProductFlags(
    const std::set<base::string16>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);

  cmd_line->AppendSwitch(switches::kChromeSxS);
  ChromeBrowserOperations::AppendProductFlags(options, cmd_line);
}

void ChromeBrowserSxSOperations::AppendRenameFlags(
    const std::set<base::string16>& options,
    CommandLine* cmd_line) const {
  DCHECK(cmd_line);

  cmd_line->AppendSwitch(switches::kChromeSxS);
  ChromeBrowserOperations::AppendRenameFlags(options, cmd_line);
}

}  // namespace installer
