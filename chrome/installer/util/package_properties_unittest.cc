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
    if (!props[i]->ReceivesUpdates()) {
      TempRegKeyOverride override(HKEY_CURRENT_USER, L"props");
      RegKey key;
      EXPECT_EQ(ERROR_SUCCESS,
          key.Create(HKEY_CURRENT_USER, state_key.c_str(), KEY_ALL_ACCESS));
    }
    TempRegKeyOverride::DeleteAllTempKeys();
  }
}
