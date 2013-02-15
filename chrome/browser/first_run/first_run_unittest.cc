// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/master_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace first_run {

class FirstRunTest : public testing::Test {
 protected:
  FirstRunTest() : user_data_dir_override_(chrome::DIR_USER_DATA) {}
  virtual ~FirstRunTest() {}

  virtual void SetUp() OVERRIDE {
    internal::GetFirstRunSentinelFilePath(&sentinel_path_);
  }

  base::FilePath sentinel_path_;

 private:
  base::ScopedPathOverride user_data_dir_override_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunTest);
};

TEST_F(FirstRunTest, RemoveSentinel) {
  EXPECT_TRUE(CreateSentinel());
  EXPECT_TRUE(file_util::PathExists(sentinel_path_));

  EXPECT_TRUE(RemoveSentinel());
  EXPECT_FALSE(file_util::PathExists(sentinel_path_));
}

TEST_F(FirstRunTest, SetupMasterPrefsFromInstallPrefs_VariationsSeed) {
  installer::MasterPreferences install_prefs("{ \"variations_seed\":\"xyz\" }");

  MasterPrefs out_prefs;
  internal::SetupMasterPrefsFromInstallPrefs(install_prefs, &out_prefs);
  EXPECT_EQ("xyz", out_prefs.variations_seed);
}

TEST_F(FirstRunTest, SetupMasterPrefsFromInstallPrefs_NoVariationsSeed) {
  installer::MasterPreferences install_prefs("{ }");

  MasterPrefs out_prefs;
  internal::SetupMasterPrefsFromInstallPrefs(install_prefs, &out_prefs);
  EXPECT_TRUE(out_prefs.variations_seed.empty());
}

}  // namespace first_run
