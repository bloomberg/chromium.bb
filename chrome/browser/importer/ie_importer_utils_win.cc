// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/ie_importer_utils_win.h"

#include "chrome/browser/importer/ie_importer_test_registry_overrider_win.h"

namespace {

const char16 kIEFavoritesOrderKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
    L"MenuOrder\\Favorites";

}

namespace importer {

base::string16 GetIEFavoritesOrderKey() {
  // Return kIEFavoritesOrderKey unless an override has been set for tests.
  base::string16 test_reg_override(
      IEImporterTestRegistryOverrider::GetTestRegistryOverride());
  return test_reg_override.empty() ? kIEFavoritesOrderKey : test_reg_override;
}

}
