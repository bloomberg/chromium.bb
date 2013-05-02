// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_oem_manifest_parser.h"

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef testing::Test KioskOemManifestParserTest;

TEST_F(KioskOemManifestParserTest, LoadTest) {
  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath kiosk_oem_file = test_data_dir.Append(FILE_PATH_LITERAL(
      "chromeos/app_mode/kiosk_manifest/kiosk_manifest.json"));
  KioskOemManifestParser::Manifest manifest;
  EXPECT_TRUE(KioskOemManifestParser::Load(kiosk_oem_file, &manifest));
  EXPECT_TRUE(manifest.enterprise_managed);
  EXPECT_FALSE(manifest.can_exit_enrollment);
  EXPECT_TRUE(manifest.keyboard_driven_oobe);
  EXPECT_EQ(manifest.device_requisition, std::string("test"));
}

}  // namespace chromeos
