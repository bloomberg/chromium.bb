// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/test_launcher_utils.h"

namespace test_launcher_utils {

bool OverrideUserDataDir(const FilePath& user_data_dir) {
  bool success = true;

  // PathService::Override() is the best way to change the user data directory.
  // This matches what is done in ChromeMain().
  success = PathService::Override(chrome::DIR_USER_DATA, user_data_dir);

#if defined(OS_LINUX)
  // Make sure the cache directory is inside our clear profile. Otherwise
  // the cache may contain data from earlier tests that could break the
  // current test.
  //
  // Note: we use an environment variable here, because we have to pass the
  // value to the child process. This is the simplest way to do it.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  success = success && env->SetVar("XDG_CACHE_HOME", user_data_dir.value());
#endif

  return success;
}

}  // namespace test_launcher_utils
