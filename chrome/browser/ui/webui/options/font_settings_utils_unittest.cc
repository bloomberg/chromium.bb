// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/font_settings_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace options {

TEST(FontSettingsUtilitiesTest, ResolveFontList) {
  EXPECT_TRUE(FontSettingsUtilities::ResolveFontList("").empty());

  // Returns the first available font if starts with ",".
  EXPECT_EQ("Arial",
            FontSettingsUtilities::ResolveFontList(",not exist, Arial"));

  // Returns the first font if no fonts are available.
  EXPECT_EQ("not exist",
            FontSettingsUtilities::ResolveFontList(",not exist, not exist 2"));

  // Otherwise returns any strings as they were set.
  std::string non_lists[] = {
      "Arial", "not exist", "not exist, Arial",
  };
  for (const std::string& name : non_lists)
    EXPECT_EQ(name, FontSettingsUtilities::ResolveFontList(name));
}

}  // namespace options
