// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/product_unittest.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/product.h"

using base::win::RegKey;
using installer::Product;
using installer::MasterPreferences;
using registry_util::RegistryOverrideManager;

void TestWithTempDir::SetUp() {
  // Name a subdirectory of the user temp directory.
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
}

void TestWithTempDir::TearDown() {
  logging::CloseLogFile();
  ASSERT_TRUE(test_dir_.Delete());
}

////////////////////////////////////////////////////////////////////////////////

void TestWithTempDirAndDeleteTempOverrideKeys::SetUp() {
  TestWithTempDir::SetUp();
}

void TestWithTempDirAndDeleteTempOverrideKeys::TearDown() {
  TestWithTempDir::TearDown();
}

////////////////////////////////////////////////////////////////////////////////

class ProductTest : public TestWithTempDirAndDeleteTempOverrideKeys {
 protected:
};

TEST_F(ProductTest, ProductInstallBasic) {
  // TODO(tommi): We should mock this and use our mocked distribution.
  const bool multi_install = false;
  const bool system_level = true;
  CommandLine cmd_line = CommandLine::FromString(
      std::wstring(L"setup.exe") +
      (multi_install ? L" --multi-install --chrome" : L"") +
      (system_level ? L" --system-level" : L""));
  installer::MasterPreferences prefs(cmd_line);
  installer::InstallationState machine_state;
  machine_state.Initialize();
  installer::InstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, machine_state);

  const Product* product = installer_state.products()[0];
  BrowserDistribution* distribution = product->distribution();
  EXPECT_EQ(BrowserDistribution::CHROME_BROWSER, distribution->GetType());

  base::FilePath user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  EXPECT_FALSE(user_data_dir.empty());

  base::FilePath program_files;
  ASSERT_TRUE(PathService::Get(base::DIR_PROGRAM_FILES, &program_files));
  // The User Data path should never be under program files, even though
  // system_level is true.
  EXPECT_EQ(std::wstring::npos,
            user_data_dir.value().find(program_files.value()));

  // There should be no installed version in the registry.
  machine_state.Initialize();
  EXPECT_TRUE(machine_state.GetProductState(
      system_level, distribution->GetType()) == NULL);

  HKEY root = installer_state.root_key();
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_pit");

    // Let's pretend chrome is installed.
    RegKey version_key(root, distribution->GetVersionKey().c_str(),
                       KEY_ALL_ACCESS);
    ASSERT_TRUE(version_key.Valid());

    const char kCurrentVersion[] = "1.2.3.4";
    Version current_version(kCurrentVersion);
    version_key.WriteValue(google_update::kRegVersionField,
                           base::UTF8ToWide(
                               current_version.GetString()).c_str());

    // We started out with a non-msi product.
    machine_state.Initialize();
    const installer::ProductState* chrome_state =
        machine_state.GetProductState(system_level, distribution->GetType());
    EXPECT_TRUE(chrome_state != NULL);
    if (chrome_state != NULL) {
      EXPECT_TRUE(chrome_state->version().Equals(current_version));
      EXPECT_FALSE(chrome_state->is_msi());
    }

    // Create a make-believe client state key.
    RegKey key;
    std::wstring state_key_path(distribution->GetStateKey());
    ASSERT_EQ(ERROR_SUCCESS,
        key.Create(root, state_key_path.c_str(), KEY_ALL_ACCESS));

    // Set the MSI marker, refresh, and verify that we now see the MSI marker.
    EXPECT_TRUE(product->SetMsiMarker(system_level, true));
    machine_state.Initialize();
    chrome_state =
        machine_state.GetProductState(system_level, distribution->GetType());
    EXPECT_TRUE(chrome_state != NULL);
    if (chrome_state != NULL)
      EXPECT_TRUE(chrome_state->is_msi());
  }
}

TEST_F(ProductTest, LaunchChrome) {
  // TODO(tommi): Test Product::LaunchChrome and
  // Product::LaunchChromeAndWait.
  LOG(ERROR) << "Test not implemented.";
}
