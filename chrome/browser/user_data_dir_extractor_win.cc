// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_data_dir_extractor_win.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/ui/user_data_dir_dialog.h"
#include "chrome/browser/user_data_dir_extractor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/main_function_params.h"

namespace chrome {

namespace {

GetUserDataDirCallback* custom_get_user_data_dir_callback = NULL;

}  // namespace

void InstallCustomGetUserDataDirCallbackForTest(
    GetUserDataDirCallback* callback) {
  custom_get_user_data_dir_callback = callback;
}

base::FilePath GetUserDataDir(const content::MainFunctionParams& parameters) {
  // If tests have installed a custom callback for GetUserDataDir(), invoke the
  // callback and return.
  if (custom_get_user_data_dir_callback)
    return custom_get_user_data_dir_callback->Run();

  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  // On Windows, if we fail to get the user data dir, bring up a dialog to
  // prompt the user to pick a different directory, and restart chrome with
  // the new dir.
  // http://code.google.com/p/chromium/issues/detail?id=11510
  if (!file_util::PathExists(user_data_dir)) {
#if defined(USE_AURA)
    // TODO(beng):
    NOTIMPLEMENTED();
#else
    base::FilePath new_user_data_dir =
        chrome::ShowUserDataDirDialog(user_data_dir);

    if (!new_user_data_dir.empty()) {
      // Because of the way CommandLine parses, it's sufficient to append a new
      // --user-data-dir switch.  The last flag of the same name wins.
      // TODO(tc): It would be nice to remove the flag we don't want, but that
      // sounds risky if we parse differently than CommandLineToArgvW.
      CommandLine new_command_line = parameters.command_line;
      new_command_line.AppendSwitchPath(switches::kUserDataDir,
                                        new_user_data_dir);
      base::LaunchProcess(new_command_line, base::LaunchOptions(), NULL);
    }
#endif
    NOTREACHED() << "Failed to get user data directory!";
  }
  return user_data_dir;
}

}  // namespace chrome
