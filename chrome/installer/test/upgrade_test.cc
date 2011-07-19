// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Boilerplate for an upgrade scenario test.  The mini_installer.exe residing in
// the same directory as the host executable is re-versioned.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/installer/test/alternate_version_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  const wchar_t kMiniInstallerExe[] = L"mini_installer.exe";
}  // namespace

// Boilerplate for a future upgrade scenario test.
class UpgradeTest : public testing::Test {
 public:
  // Generate a newer version of mini_installer.exe.
  static void SetUpTestCase() {
    FilePath dir_exe;
    ASSERT_TRUE(PathService::Get(base::DIR_EXE, &dir_exe));
    ASSERT_TRUE(file_util::CreateTemporaryFile(&next_mini_installer_path_));
    ASSERT_TRUE(
        upgrade_test::GenerateAlternateVersion(
            dir_exe.Append(&kMiniInstallerExe[0]),
            next_mini_installer_path_,
            upgrade_test::NEXT_VERSION, NULL, NULL));
  }

  // Clean up by deleting the created newer version of mini_installer.exe.
  static void TearDownTestCase() {
    EXPECT_TRUE(file_util::Delete(next_mini_installer_path_, false));
  }
 private:
  static FilePath next_mini_installer_path_;
};  // class UpgradeTest

FilePath UpgradeTest::next_mini_installer_path_;

TEST_F(UpgradeTest, DoNothing) {
}
