// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/package_properties.h"
#include "chrome/installer/util/product_unittest.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using installer::PackageProperties;
using installer::ChromePackageProperties;
using installer::ChromiumPackageProperties;

class PackagePropertiesTest : public testing::Test {
 protected:
};

TEST_F(PackagePropertiesTest, Basic) {
  TempRegKeyOverride::DeleteAllTempKeys();
  ChromePackageProperties chrome_props;
  ChromiumPackageProperties chromium_props;
  PackageProperties* props[] = { &chrome_props, &chromium_props };
  for (size_t i = 0; i < arraysize(props); ++i) {
    std::wstring state_key(props[i]->GetStateKey());
    EXPECT_FALSE(state_key.empty());
    std::wstring version_key(props[i]->GetVersionKey());
    EXPECT_FALSE(version_key.empty());
    if (props[i]->ReceivesUpdates()) {
      HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
      for (size_t j = 0; j < arraysize(roots); ++j) {
        {
          TempRegKeyOverride override(roots[j], L"props");
          RegKey key;
          EXPECT_TRUE(key.Create(roots[j], state_key.c_str(), KEY_ALL_ACCESS));
          EXPECT_TRUE(key.WriteValue(google_update::kRegApField, L""));
          props[i]->UpdateDiffInstallStatus(roots[j] == HKEY_LOCAL_MACHINE,
                                            true,
                                            installer::INSTALL_FAILED);
          std::wstring value;
          key.ReadValue(google_update::kRegApField, &value);
          EXPECT_FALSE(value.empty());
        }
        TempRegKeyOverride::DeleteAllTempKeys();
      }
    } else {
      TempRegKeyOverride override(HKEY_CURRENT_USER, L"props");
      RegKey key;
      EXPECT_TRUE(key.Create(HKEY_CURRENT_USER, state_key.c_str(),
                             KEY_ALL_ACCESS));
    }
    TempRegKeyOverride::DeleteAllTempKeys();
  }
}
