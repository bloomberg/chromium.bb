// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_handle.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/package.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/product_unittest.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

using base::win::RegKey;
using base::win::ScopedHandle;
using installer::Package;
using installer::Product;
using installer::Version;

class PackageTest : public TestWithTempDirAndDeleteTempOverrideKeys {
 protected:
};

// Tests a few basic things of the Package class.  Makes sure that the path
// operations are correct
TEST_F(PackageTest, Basic) {
  scoped_refptr<Package> package(new Package(test_dir_.path()));
  EXPECT_EQ(test_dir_.path().value(), package->path().value());
  EXPECT_TRUE(package->IsEqual(test_dir_.path()));
  EXPECT_EQ(0U, package->products().size());

  const wchar_t kOldVersion[] = L"1.2.3.4";
  const wchar_t kNewVersion[] = L"2.3.4.5";

  scoped_ptr<Version> new_version(Version::GetVersionFromString(kNewVersion));
  scoped_ptr<Version> old_version(Version::GetVersionFromString(kOldVersion));
  ASSERT_TRUE(new_version.get() != NULL);
  ASSERT_TRUE(old_version.get() != NULL);

  FilePath installer_dir(package->GetInstallerDirectory(*new_version.get()));
  EXPECT_FALSE(installer_dir.empty());

  FilePath new_version_dir(package->path().Append(new_version->GetString()));
  FilePath old_version_dir(package->path().Append(old_version->GetString()));

  EXPECT_FALSE(file_util::PathExists(new_version_dir));
  EXPECT_FALSE(file_util::PathExists(old_version_dir));

  EXPECT_FALSE(file_util::PathExists(installer_dir));
  file_util::CreateDirectory(installer_dir);
  EXPECT_TRUE(file_util::PathExists(new_version_dir));

  file_util::CreateDirectory(old_version_dir);
  EXPECT_TRUE(file_util::PathExists(old_version_dir));

  // Create a fake chrome.dll key file in the old version directory.  This
  // should prevent the old version directory from getting deleted.
  FilePath old_chrome_dll(old_version_dir.Append(installer::kChromeDll));
  EXPECT_FALSE(file_util::PathExists(old_chrome_dll));

  // Hold on to the file exclusively to prevent the directory from
  // being deleted.
  ScopedHandle file(::CreateFile(old_chrome_dll.value().c_str(), GENERIC_READ,
                                 0, NULL, OPEN_ALWAYS, 0, NULL));
  EXPECT_TRUE(file.IsValid());
  EXPECT_TRUE(file_util::PathExists(old_chrome_dll));

  package->RemoveOldVersionDirectories(*new_version.get());
  // The old directory should still exist.
  EXPECT_TRUE(file_util::PathExists(old_version_dir));
  EXPECT_TRUE(file_util::PathExists(new_version_dir));

  // Now close the file handle to make it possible to delete our key file.
  file.Close();

  package->RemoveOldVersionDirectories(*new_version.get());
  // The new directory should still exist.
  EXPECT_TRUE(file_util::PathExists(new_version_dir));

  // Now, the old directory and key file should be gone.
  EXPECT_FALSE(file_util::PathExists(old_chrome_dll));
  EXPECT_FALSE(file_util::PathExists(old_version_dir));
}

TEST_F(PackageTest, WithProduct) {
  TempRegKeyOverride::DeleteAllTempKeys();

  const installer::MasterPreferences& prefs =
      installer::MasterPreferences::ForCurrentProcess();

  // TODO(tommi): We should mock this and use our mocked distribution.
  const bool system_level = true;
  BrowserDistribution* distribution =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER, prefs);
  scoped_refptr<Package> package(new Package(test_dir_.path()));
  scoped_refptr<Product> product(new Product(distribution,
                                             system_level,
                                             package.get()));
  EXPECT_EQ(1U, package->products().size());
  EXPECT_EQ(system_level, package->system_level());

  const wchar_t kCurrentVersion[] = L"1.2.3.4";
  scoped_ptr<Version> current_version(
      Version::GetVersionFromString(kCurrentVersion));

  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  {
    TempRegKeyOverride override(root, L"root_pit");
    RegKey chrome_key(root, distribution->GetVersionKey().c_str(),
                      KEY_ALL_ACCESS);
    EXPECT_TRUE(chrome_key.Valid());
    if (chrome_key.Valid()) {
      chrome_key.WriteValue(google_update::kRegVersionField,
                            current_version->GetString().c_str());
      // TODO(tommi): Also test for when there exists a new_chrome.exe.
      scoped_ptr<Version> found_version(package->GetCurrentVersion());
      EXPECT_TRUE(found_version.get() != NULL);
      if (found_version.get()) {
        EXPECT_TRUE(current_version->IsEqual(*found_version.get()));
      }
    }
  }

  TempRegKeyOverride::DeleteAllTempKeys();
}
