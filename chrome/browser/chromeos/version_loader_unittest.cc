// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

TEST_F(VersionLoaderTest, ParseVersion) {
  EXPECT_EQ("0.2.3.3", VersionLoader::ParseVersion(kTest1));
  EXPECT_EQ("0.2.3.3", VersionLoader::ParseVersion(kTest2));
  EXPECT_EQ("0.2.3.3", VersionLoader::ParseVersion(kTest3));
  EXPECT_EQ("\"", VersionLoader::ParseVersion(kTest4));
  EXPECT_EQ(std::string(), VersionLoader::ParseVersion(kTest5));
  EXPECT_EQ(std::string(), VersionLoader::ParseVersion(std::string()));
}

}  // namespace chromeos
