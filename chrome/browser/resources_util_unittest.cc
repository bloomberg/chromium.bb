// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources_util.h"

#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestCase {
  const char* name;
  int id;
};

}  // namespace

TEST(ResourcesUtil, SpotCheckIds) {
  const TestCase kTestCases[] = {
    {"IDR_BACK", IDR_BACK},
    {"IDR_STOP", IDR_STOP},
    {"IDR_OMNIBOX_STAR", IDR_OMNIBOX_STAR},
    {"IDR_SAD_TAB", IDR_SAD_TAB},
  };
  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    EXPECT_EQ(kTestCases[i].id,
              ResourcesUtil::GetThemeResourceId(kTestCases[i].name));
  }

  // Should return -1 of unknown names.
  EXPECT_EQ(-1, ResourcesUtil::GetThemeResourceId("foobar"));
  EXPECT_EQ(-1, ResourcesUtil::GetThemeResourceId("backstar"));
}
