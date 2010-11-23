// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include <stdlib.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test the behavior of chrome::GetUserCacheDirectory.
// See that function's comments for discussion of the subtleties.
TEST(ChromePaths, UserCacheDir) {
  FilePath test_profile_dir, cache_dir;
#if defined(OS_MACOSX)
  ASSERT_TRUE(PathService::Get(base::DIR_APP_DATA, &test_profile_dir));
  test_profile_dir = test_profile_dir.Append("foobar");
  FilePath expected_cache_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_CACHE, &expected_cache_dir));
  expected_cache_dir = expected_cache_dir.Append("foobar");
#elif(OS_POSIX)
  FilePath homedir = file_util::GetHomeDir();
  // Note: we assume XDG_CACHE_HOME/XDG_CONFIG_HOME are at their
  // default settings.
  test_profile_dir = homedir.Append(".config/foobar");
  FilePath expected_cache_dir = homedir.Append(".cache/foobar");
#endif

  // Verify that a profile in the special platform-specific source
  // location ends up in the special target location.
#if !defined(OS_WIN)  // No special behavior on Windows.
  chrome::GetUserCacheDirectory(test_profile_dir, &cache_dir);
  EXPECT_EQ(expected_cache_dir.value(), cache_dir.value());
#endif

  // Verify that a profile in some other random directory doesn't use
  // the special cache dir.
  test_profile_dir = FilePath(FILE_PATH_LITERAL("/some/other/path"));
  chrome::GetUserCacheDirectory(test_profile_dir, &cache_dir);
  EXPECT_EQ(test_profile_dir.value(), cache_dir.value());
}
