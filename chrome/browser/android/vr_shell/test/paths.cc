// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/test/paths.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace vr_shell {
namespace test {

namespace {

bool PathProvider(int key, base::FilePath* result) {
  base::FilePath cur;
  switch (key) {
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case DIR_TEST_DATA:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("chrome"))
                .Append(FILE_PATH_LITERAL("browser"))
                .Append(FILE_PATH_LITERAL("android"))
                .Append(FILE_PATH_LITERAL("vr_shell"))
                .Append(FILE_PATH_LITERAL("test"))
                .Append(FILE_PATH_LITERAL("data"));
      if (!base::PathExists(cur))
        return false;
      break;
    default:
      return false;
  }
  *result = cur;
  return true;
}

}  // namespace

void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace test
}  // namespace vr_shell
