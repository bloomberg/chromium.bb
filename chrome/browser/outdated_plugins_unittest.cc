// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/outdated_plugins.h"
#include "testing/gtest/include/gtest/gtest.h"

class OutdatedPluginsTest : public testing::Test {
};

TEST_F(OutdatedPluginsTest, CheckFlashVersion) {
  // In the case that we can't parse the version, assume that it's ok.
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"bogus value"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"1.2.3"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"1x.2.3.4"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"1.2x.3.4"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"1.2.3x.4"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"1.2.3.4x"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"1.2.3.4.5"));

  // Very out of date versions should trigger
  EXPECT_TRUE(OutdatedPlugins::CheckFlashVersion(L"1.2.3.4"));
  EXPECT_TRUE(OutdatedPlugins::CheckFlashVersion(L"2.0.0.0"));
  EXPECT_TRUE(OutdatedPlugins::CheckFlashVersion(L"8.0.0.0"));

  // Known bad versions
  EXPECT_TRUE(OutdatedPlugins::CheckFlashVersion(L"9.0.159.0"));
  EXPECT_TRUE(OutdatedPlugins::CheckFlashVersion(L"9.0.158.0"));

  EXPECT_TRUE(OutdatedPlugins::CheckFlashVersion(L"10.0.22.87"));
  EXPECT_TRUE(OutdatedPlugins::CheckFlashVersion(L"10.0.21.87"));
  EXPECT_TRUE(OutdatedPlugins::CheckFlashVersion(L"10.0.22.88"));

  // Known good versions
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"10.0.32.18"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"9.0.246.0"));

  // Version numbers from the future
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"10.0.32.19"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"10.0.33.0"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"10.0.33.20"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"10.1.31.0"));
  EXPECT_FALSE(OutdatedPlugins::CheckFlashVersion(L"11.0.0.0"));
}
