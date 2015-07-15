// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/scoped_user_data_dir.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "mojo/runner/switches.h"

namespace mojo {
namespace runner {

ScopedUserDataDir::ScopedUserDataDir() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kUseTemporaryUserDataDir))
    return;

  if (command_line->HasSwitch(switches::kUserDataDir)) {
    // User should not specify a --user-data-dir manually when using
    // --use-temporary-user-data-dir. The point of the flag is to let the
    // mojo runner process manage the lifetime of the user data dir.
    LOG(ERROR) << "Ignoring request to --use-temporary-user-data-dir because "
               << "--user-data-dir was also specified.";
    return;
  }

  if (!temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create a temporary user data dir.";
    return;
  }

  command_line->AppendSwitchPath(switches::kUserDataDir, temp_dir_.path());
}

ScopedUserDataDir::~ScopedUserDataDir() {
}

}  // namespace runner
}  // namespace mojo
