// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/mac/app_mode_chrome_locator.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Return the path to the Chrome/Chromium app bundle compiled along with the
// test executable.
void GetChromeBundlePath(base::FilePath* chrome_bundle) {
  base::FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kBrowserProcessExecutableNameChromium);
  path = path.ReplaceExtension(base::FilePath::StringType("app"));
  *chrome_bundle = path;
}

}  // namespace

TEST(ChromeLocatorTest, FindBundle) {
  base::FilePath finder_bundle_path;
  EXPECT_TRUE(
      app_mode::FindBundleById(@"com.apple.finder", &finder_bundle_path));
  EXPECT_TRUE(base::DirectoryExists(finder_bundle_path));
}

TEST(ChromeLocatorTest, FindNonExistentBundle) {
  base::FilePath dummy;
  EXPECT_FALSE(app_mode::FindBundleById(@"this.doesnt.exist", &dummy));
}

TEST(ChromeLocatorTest, GetNonExistentBundleInfo) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath executable_path;
  base::string16 raw_version;
  base::FilePath version_path;
  base::FilePath framework_path;
  EXPECT_FALSE(app_mode::GetChromeBundleInfo(temp_dir.path(),
      &executable_path, &raw_version, &version_path, &framework_path));
}

TEST(ChromeLocatorTest, GetChromeBundleInfo) {
  using app_mode::GetChromeBundleInfo;

  base::FilePath chrome_bundle_path;
  GetChromeBundlePath(&chrome_bundle_path);
  ASSERT_TRUE(base::DirectoryExists(chrome_bundle_path));

  base::FilePath executable_path;
  base::string16 raw_version;
  base::FilePath version_path;
  base::FilePath framework_path;
  EXPECT_TRUE(GetChromeBundleInfo(chrome_bundle_path,
      &executable_path, &raw_version, &version_path, &framework_path));
  EXPECT_TRUE(base::PathExists(executable_path));
  EXPECT_GT(raw_version.size(), 0U);
  EXPECT_TRUE(base::DirectoryExists(version_path));
  EXPECT_TRUE(base::PathExists(framework_path));
}
