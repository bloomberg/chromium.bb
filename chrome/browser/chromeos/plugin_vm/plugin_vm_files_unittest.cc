// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

class PluginVmFilesTest : public testing::Test {
 public:
  void Callback(bool expected, bool result) { EXPECT_EQ(result, expected); }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  }

  void TearDown() override { profile_.reset(); }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(PluginVmFilesTest, DirNotExists) {
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  EnsureDefaultSharedDirExists(profile_.get(),
                               base::BindOnce(&PluginVmFilesTest::Callback,
                                              base::Unretained(this), true));
  thread_bundle_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, DirAlreadyExists) {
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  base::CreateDirectory(my_files.Append("PluginVm"));
  EnsureDefaultSharedDirExists(profile_.get(),
                               base::BindOnce(&PluginVmFilesTest::Callback,
                                              base::Unretained(this), true));
  thread_bundle_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, FileAlreadyExists) {
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  base::FilePath path = my_files.Append("PluginVm");
  EXPECT_TRUE(base::CreateDirectory(my_files));
  EXPECT_EQ(base::WriteFile(path, "", 0), 0);
  EnsureDefaultSharedDirExists(profile_.get(),
                               base::BindOnce(&PluginVmFilesTest::Callback,
                                              base::Unretained(this), false));
  thread_bundle_.RunUntilIdle();
}

}  // namespace plugin_vm
