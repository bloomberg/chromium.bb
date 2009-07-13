// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "app/app_paths.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"

namespace app {

bool PathProvider(int key, FilePath* result) {
  // Assume that we will not need to create the directory if it does not exist.
  // This flag can be set to true for the cases where we want to create it.
  bool create_dir = false;

  FilePath cur;
  switch (key) {
#if !defined(OS_MACOSX)
    // These are not "themes" that are user-created, but rather the dlls and
    // pak files. On the Mac, we keep the pak files in the lproj folders.
    case app::DIR_THEMES:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("themes"));
      create_dir = true;
      break;
#endif
    case app::DIR_LOCALES:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
#if defined(OS_MACOSX)
      // On Mac, locale files are in Contents/Resources, a sibling of the
      // App dir.
      cur = cur.DirName();
      cur = cur.Append(FILE_PATH_LITERAL("Resources"));
#else
      cur = cur.Append(FILE_PATH_LITERAL("locales"));
#endif
      create_dir = true;
      break;
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
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case app::DIR_TEST_DATA:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("app"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
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
