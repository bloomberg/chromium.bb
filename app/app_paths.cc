// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/app_paths.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace app {

bool PathProvider(int key, FilePath* result) {
  // Assume that we will not need to create the directory if it does not exist.
  // This flag can be set to true for the cases where we want to create it.
  bool create_dir = false;

  FilePath cur;
  switch (key) {
    case app::DIR_EXTERNAL_EXTENSIONS:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
#if defined(OS_MACOSX)
      // On Mac, built-in extensions are in Contents/Extensions, a sibling of
      // the App dir. If there are none, it may not exist.
      cur = cur.DirName();
      cur = cur.Append(FILE_PATH_LITERAL("Extensions"));
      create_dir = false;
#else
      cur = cur.Append(FILE_PATH_LITERAL("extensions"));
      create_dir = true;
#endif
      break;
    default:
      return false;
  }

  if (create_dir && !file_util::PathExists(cur) &&
      !file_util::CreateDirectory(cur))
    return false;

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace app
