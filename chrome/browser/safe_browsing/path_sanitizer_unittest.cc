// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/path_sanitizer.h"

#include <vector>

#include "base/logging.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Returns the root directory with a trailing separator. Works on all platforms.
base::FilePath GetRootDirectory() {
  base::FilePath dir_temp;
  if (!PathService::Get(base::DIR_TEMP, &dir_temp))
    NOTREACHED();

  std::vector<base::FilePath::StringType> components;
  dir_temp.GetComponents(&components);

  return base::FilePath(components[0]).AsEndingWithSeparator();
}

}  // namespace

TEST(SafeBrowsingPathSanitizerTest, HomeDirectoryIsNotEmpty) {
  safe_browsing::PathSanitizer path_sanitizer;

  ASSERT_FALSE(path_sanitizer.GetHomeDirectory().empty());
}

TEST(SafeBrowsingPathSanitizerTest, DontStripHomeDirectoryTest) {
  // Test with path not in home directory.
  base::FilePath path =
      GetRootDirectory().Append(FILE_PATH_LITERAL("not_in_home_directory.ext"));
  base::FilePath path_expected = path;

  safe_browsing::PathSanitizer path_sanitizer;
  path_sanitizer.StripHomeDirectory(&path);

  ASSERT_EQ(path.value(), path_expected.value());
}

TEST(SafeBrowsingPathSanitizerTest, DoStripHomeDirectoryTest) {
  // Test with path in home directory.
  safe_browsing::PathSanitizer path_sanitizer;

  base::FilePath path = path_sanitizer.GetHomeDirectory().Append(
      FILE_PATH_LITERAL("in_home_directory.ext"));
  base::FilePath path_expected = base::FilePath(FILE_PATH_LITERAL("~")).Append(
      FILE_PATH_LITERAL("in_home_directory.ext"));

  path_sanitizer.StripHomeDirectory(&path);

  ASSERT_EQ(path.value(), path_expected.value());
}
