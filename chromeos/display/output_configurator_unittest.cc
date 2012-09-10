// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/output_configurator.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef testing::Test OutputConfiguratorTest;

TEST_F(OutputConfiguratorTest, IsInternalOutputName) {
  EXPECT_TRUE(OutputConfigurator::IsInternalOutputName("LVDS"));
  EXPECT_TRUE(OutputConfigurator::IsInternalOutputName("eDP"));
  EXPECT_TRUE(OutputConfigurator::IsInternalOutputName("LVDSxx"));
  EXPECT_TRUE(OutputConfigurator::IsInternalOutputName("eDPzz"));

  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("xyz"));
  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("abcLVDS"));
  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("cdeeDP"));
  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("LVD"));
  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("eD"));
}

}  // namespace chromeos
