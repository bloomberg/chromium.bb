// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/version_loader.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef testing::Test VersionLoaderTest;

static const char kTest1[] =
    "DIST_ID=1\nCHROMEOS_RELEASE_DESCRIPTION=0.2.3.3\nFOO=BAR";
static const char kTest2[] = "DIST_ID=1\nCHROMEOS_RELEASE_DESCRIPTION=0.2.3.3";
static const char kTest3[] = "CHROMEOS_RELEASE_DESCRIPTION=\"0.2.3.3\"";
static const char kTest4[] = "CHROMEOS_RELEASE_DESCRIPTION=\"\"\"";
static const char kTest5[] = "CHROMEOS_RELEASE_DESCRIPTION=\"\"";

static const char kTest6[] =
    "CHROMEOS_RELEASE_DESCRIPTION=1\nCHROMEOS_RELEASE_VERSION=0.2.3.3\nFOO=BAR";
static const char kTest7[] = "DIST_ID=1\nCHROMEOS_RELEASE_VERSION=0.2.3.3";
static const char kTest8[] = "CHROMEOS_RELEASE_VERSION=\"0.2.3.3\"";
static const char kTest9[] = "CHROMEOS_RELEASE_VERSION=\"\"";

static const char kTest10[] = "vendor            | FOO";
static const char kTest11[] = "firmware          | FOO";
static const char kTest12[] = "firmware          | FOO";
static const char kTest13[] = "version           | 0.2.3.3";
static const char kTest14[] = "version        | 0.2.3.3";
static const char kTest15[] = "version             0.2.3.3";

TEST_F(VersionLoaderTest, ParseFullVersion) {
  EXPECT_EQ("0.2.3.3",
            VersionLoader::ParseVersion(kTest1,
                                        VersionLoader::kFullVersionPrefix));
  EXPECT_EQ("0.2.3.3",
            VersionLoader::ParseVersion(kTest2,
                                        VersionLoader::kFullVersionPrefix));
  EXPECT_EQ("0.2.3.3",
            VersionLoader::ParseVersion(kTest3,
                                        VersionLoader::kFullVersionPrefix));
  EXPECT_EQ("\"",
            VersionLoader::ParseVersion(kTest4,
                                        VersionLoader::kFullVersionPrefix));
  EXPECT_EQ(std::string(),
            VersionLoader::ParseVersion(kTest5,
                                        VersionLoader::kFullVersionPrefix));
  EXPECT_EQ(std::string(),
            VersionLoader::ParseVersion(std::string(),
                                        VersionLoader::kFullVersionPrefix));
}

TEST_F(VersionLoaderTest, ParseVersion) {
  EXPECT_EQ("0.2.3.3",
            VersionLoader::ParseVersion(kTest6,
                                        VersionLoader::kVersionPrefix));
  EXPECT_EQ("0.2.3.3",
            VersionLoader::ParseVersion(kTest7,
                                        VersionLoader::kVersionPrefix));
  EXPECT_EQ("0.2.3.3",
            VersionLoader::ParseVersion(kTest8,
                                        VersionLoader::kVersionPrefix));
  EXPECT_EQ(std::string(),
            VersionLoader::ParseVersion(kTest9,
                                        VersionLoader::kFullVersionPrefix));
}

TEST_F(VersionLoaderTest, ParseFirmware) {
  EXPECT_EQ(std::string(), VersionLoader::ParseFirmware(kTest10));
  EXPECT_EQ(std::string(), VersionLoader::ParseFirmware(kTest11));
  EXPECT_EQ(std::string(), VersionLoader::ParseFirmware(kTest12));
  EXPECT_EQ("0.2.3.3", VersionLoader::ParseFirmware(kTest13));
  EXPECT_EQ("0.2.3.3", VersionLoader::ParseFirmware(kTest14));
  EXPECT_EQ("0.2.3.3", VersionLoader::ParseFirmware(kTest15));
}

}  // namespace chromeos
