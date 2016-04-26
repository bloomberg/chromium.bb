// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/version.h"
#include "chrome/tools/crash_service/caps/exit_codes.h"
#include "chrome/tools/crash_service/caps/logger_win.h"
#include "chrome/tools/crash_service/caps/process_singleton_win.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  base::AtExitManager at_exit_manager;
  base::FilePath dir_exe;
  if (!PathService::Get(base::DIR_EXE, &dir_exe))
    return caps::EC_INIT_ERROR;

  // What directory we write depends if we are being run from the actual
  // location (a versioned directory) or from a build output directory.
  base::Version version(dir_exe.BaseName().MaybeAsASCII());
  auto data_path = version.IsValid() ? dir_exe.DirName() : dir_exe;

  // Start logging.
  caps::Logger logger(data_path);

  // Check for an existing running instance.
  caps::ProcessSingleton process_singleton;
  if (process_singleton.other_instance())
    return caps::EC_EXISTING_INSTANCE;

  return caps::EC_NORMAL_EXIT;
}

