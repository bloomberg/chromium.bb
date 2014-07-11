// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_paths.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"

namespace chromecast {

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case DIR_CAST_HOME: {
      base::FilePath home = base::GetHomeDir();
#if defined(ARCH_CPU_ARMEL)
      // When running on the actual device, $HOME is set to the user's
      // directory under the data partition.
      *result = home;
#else
      // When running a development instance as a regular user, use
      // a data directory under $HOME (similar to Chrome).
      *result = home.Append(".config/cast_shell");
#endif
      return true;
    }
  }
  return false;
}

void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace chromecast
