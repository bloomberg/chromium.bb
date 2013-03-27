// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_data_dir_extractor.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

namespace chrome {

base::FilePath GetUserDataDir(const content::MainFunctionParams& parameters) {
  base::FilePath user_data_dir;

  // Getting the user data dir can fail if the directory isn't creatable, for
  // example: on Windows we bring up a dialog prompting the user to pick a
  // different directory. However, ProcessSingleton needs a real user_data_dir
  // on Mac/Linux, so it's better to fail here than fail mysteriously elsewhere.
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      << "Must be able to get user data directory!";
  return user_data_dir;
}

}  // namespace chrome
