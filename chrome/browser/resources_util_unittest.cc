// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources_util.h"

#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ResourcesUtil, SpotCheckIds) {
  const struct {
    const char* name;
    int id;
  } kCases[] = {
    // IDRs from chrome/app/theme/theme_resources.grd should be valid.
    {"IDR_BACK", IDR_BACK},
    {"IDR_STOP", IDR_STOP},
    // IDRs from ui/resources/ui_resources.grd should be valid.
    {"IDR_CHECKMARK", IDR_CHECKMARK},
    {"IDR_THROBBER", IDR_THROBBER},
    // Unknown names should be invalid and return -1.
    {"foobar", -1},
    {"backstar", -1},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kCases); ++i)
    EXPECT_EQ(kCases[i].id, ResourcesUtil::GetThemeResourceId(kCases[i].name));
}
