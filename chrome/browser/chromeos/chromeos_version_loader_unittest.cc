// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromeos_version_loader.h"

#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test ChromeOSVersionLoaderTest;

static const char kTest1[] = "DIST_ID=1\nGOOGLE_RELEASE=0.2.3.3\nFOO=BAR";
static const char kTest2[] = "DIST_ID=1\nGOOGLE_RELEASE=0.2.3.3";
static const char kTest3[] = "GOOGLE_RELEASE=\"0.2.3.3\"";
static const char kTest4[] = "GOOGLE_RELEASE=\"\"\"";
static const char kTest5[] = "GOOGLE_RELEASE=\"\"";

TEST_F(ChromeOSVersionLoaderTest, ParseVersion) {
  EXPECT_EQ("0.2.3.3", ChromeOSVersionLoader::ParseVersion(kTest1));
  EXPECT_EQ("0.2.3.3", ChromeOSVersionLoader::ParseVersion(kTest2));
  EXPECT_EQ("0.2.3.3", ChromeOSVersionLoader::ParseVersion(kTest3));
  EXPECT_EQ("\"", ChromeOSVersionLoader::ParseVersion(kTest4));
  EXPECT_EQ(std::string(), ChromeOSVersionLoader::ParseVersion(kTest5));
  EXPECT_EQ(std::string(), ChromeOSVersionLoader::ParseVersion(std::string()));
}
