// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

namespace {

TEST(SearchTest, EmbeddedSearchAPIEnabled) {
  EXPECT_EQ(1ul, EmbeddedSearchPageVersion());
  EXPECT_FALSE(IsInstantExtendedAPIEnabled());
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableEmbeddedSearchAPI);
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
}

}  // namespace

}  // namespace chrome
