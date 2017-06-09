// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/test/paths.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr_shell {
namespace test {

void GetTestDataPath(base::FilePath* result) {
  base::FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &path));
  path = path.Append(FILE_PATH_LITERAL("chrome"))
             .Append(FILE_PATH_LITERAL("browser"))
             .Append(FILE_PATH_LITERAL("android"))
             .Append(FILE_PATH_LITERAL("vr_shell"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"));
  ASSERT_TRUE(base::PathExists(path));
  *result = path;
}

}  // namespace test
}  // namespace vr_shell
