// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/package.h"
#include "chrome/installer/util/package_properties.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/product_unittest.h"
#include "chrome/installer/util/util_constants.h"

using base::win::RegKey;
using installer::ChromePackageProperties;
using installer::ChromiumPackageProperties;
using installer::Package;
using installer::Product;
using installer::MasterPreferences;

class PackageTest : public TestWithTempDirAndDeleteTempOverrideKeys {
 protected:
};

// Tests a few basic things of the Package class.  Makes sure that the path
// operations are correct
TEST_F(PackageTest, Basic) {
  const bool multi_install = false;
  const bool system_level = true;
  ChromiumPackageProperties properties;
  scoped_refptr<Package> package(new Package(multi_install, system_level,
                                             test_dir_.path(), &properties));
  EXPECT_EQ(test_dir_.path().value(), package->path().value());
  EXPECT_TRUE(package->IsEqual(test_dir_.path()));
  EXPECT_EQ(0U, package->products().size());

  const char kOldVersion[] = "1.2.3.4";
  const char kNewVersion[] = "2.3.4.5";

  scoped_ptr<Version> new_version(Version::GetVersionFromString(kNewVersion));
  scoped_ptr<Version> old_version(Version::GetVersionFromString(kOldVersion));
  ASSERT_TRUE(new_version.get() != NULL);
  ASSERT_TRUE(old_version.get() != NULL);

  FilePath installer_dir(package->GetInstallerDirectory(*new_version.get()));
  EXPECT_FALSE(installer_dir.empty());

  FilePath new_version_dir(package->path().Append(
      UTF8ToWide(new_version->GetString())));
  FilePath old_version_dir(package->path().Append(
      UTF8ToWide(old_version->GetString())));

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
  base::win::ScopedHandle file(
    ::CreateFile(old_chrome_dll.value().c_str(), GENERIC_READ,
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
  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();

  // TODO(tommi): We should mock this and use our mocked distribution.
  const bool multi_install = false;
  const bool system_level = true;
  BrowserDistribution* distribution =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER, prefs);
  ChromePackageProperties properties;
  scoped_refptr<Package> package(new Package(multi_install, system_level,
                                             test_dir_.path(), &properties));
  scoped_refptr<Product> product(new Product(distribution, package.get()));
  EXPECT_EQ(1U, package->products().size());
  EXPECT_EQ(system_level, package->system_level());

  const char kCurrentVersion[] = "1.2.3.4";
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
                            UTF8ToWide(current_version->GetString()).c_str());
      // TODO(tommi): Also test for when there exists a new_chrome.exe.
      scoped_ptr<Version> found_version(package->GetCurrentVersion());
      EXPECT_TRUE(found_version.get() != NULL);
      if (found_version.get()) {
        EXPECT_TRUE(current_version->Equals(*found_version));
      }
    }
  }
}

namespace {
bool SetUninstallArguments(HKEY root, BrowserDistribution* dist,
                           const CommandLine& args) {
  RegKey key(root, dist->GetStateKey().c_str(), KEY_ALL_ACCESS);
  return key.WriteValue(installer::kUninstallArgumentsField,
                        args.command_line_string().c_str());
}

bool SetInstalledVersion(HKEY root, BrowserDistribution* dist,
                         const std::wstring& version) {
  RegKey key(root, dist->GetVersionKey().c_str(), KEY_ALL_ACCESS);
  return key.WriteValue(google_update::kRegVersionField, version.c_str());
}
}  // end namespace

TEST_F(PackageTest, Dependency) {
  const bool multi_install = false;
  const bool system_level = true;
  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  TempRegKeyOverride override(root, L"root_dep");

  ChromePackageProperties properties;
  scoped_refptr<Package> package(new Package(multi_install, system_level,
                                             test_dir_.path(), &properties));
  EXPECT_EQ(0U, package->GetMultiInstallDependencyCount());

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();

  BrowserDistribution* chrome = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BROWSER, prefs);
  BrowserDistribution* cf = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME, prefs);

  CommandLine multi_uninstall_cmd(CommandLine::NO_PROGRAM);
  multi_uninstall_cmd.AppendSwitch(installer::switches::kUninstall);
  multi_uninstall_cmd.AppendSwitch(installer::switches::kMultiInstall);

  CommandLine single_uninstall_cmd(CommandLine::NO_PROGRAM);
  single_uninstall_cmd.AppendSwitch(installer::switches::kUninstall);

  // "install" Chrome.
  SetUninstallArguments(root, chrome, multi_uninstall_cmd);
  SetInstalledVersion(root, chrome, L"1.2.3.4");
  EXPECT_EQ(1U, package->GetMultiInstallDependencyCount());

  // "install" Chrome Frame without multi-install.
  SetUninstallArguments(root, cf, single_uninstall_cmd);
  SetInstalledVersion(root, cf, L"1.2.3.4");
  EXPECT_EQ(1U, package->GetMultiInstallDependencyCount());

  // "install" Chrome Frame with multi-install.
  SetUninstallArguments(root, cf, multi_uninstall_cmd);
  EXPECT_EQ(2U, package->GetMultiInstallDependencyCount());
}
