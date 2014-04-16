// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/login/fake_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace util {

class FileSystemProviderMountPathUtilTest : public testing::Test {
 protected:
  FileSystemProviderMountPathUtilTest() {}
  virtual ~FileSystemProviderMountPathUtilTest() {}

  virtual void SetUp() OVERRIDE {
    user_manager_ = new FakeUserManager();
    user_manager_enabler_.reset(new ScopedUserManagerEnabler(user_manager_));
    profile_.reset(new TestingProfile);
    user_manager_->AddUser(profile_->GetProfileName());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  FakeUserManager* user_manager_;
};

TEST_F(FileSystemProviderMountPathUtilTest, GetMountPointPath) {
  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  const int kFileSystemId = 1;

  base::FilePath result =
      GetMountPointPath(profile_.get(), kExtensionId, kFileSystemId);
  EXPECT_EQ("/provided/mbflcebpggnecokmikipoihdbecnjfoj-1-testing_profile-hash",
            result.AsUTF8Unsafe());
}

}  // namespace util
}  // namespace file_system_provider
}  // namespace chromeos
