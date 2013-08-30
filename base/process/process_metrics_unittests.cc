// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"


namespace base {
namespace debug {

// Tests for SystemMetrics.
// Exists as a class so it can be a friend of SystemMetrics.
class SystemMetricsTest : public testing::Test {
 public:
  SystemMetricsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemMetricsTest);
};

/////////////////////////////////////////////////////////////////////////////

#if defined(OS_LINUX) || defined(OS_ANDROID)
TEST_F(SystemMetricsTest, IsValidDiskName) {
  std::string invalid_input1 = "";
  std::string invalid_input2 = "s";
  std::string invalid_input3 = "sdz+";
  std::string invalid_input4 = "hda0";
  std::string invalid_input5 = "mmcbl";
  std::string invalid_input6 = "mmcblka";
  std::string invalid_input7 = "mmcblkb";
  std::string invalid_input8 = "mmmblk0";

  EXPECT_FALSE(IsValidDiskName(invalid_input1));
  EXPECT_FALSE(IsValidDiskName(invalid_input2));
  EXPECT_FALSE(IsValidDiskName(invalid_input3));
  EXPECT_FALSE(IsValidDiskName(invalid_input4));
  EXPECT_FALSE(IsValidDiskName(invalid_input5));
  EXPECT_FALSE(IsValidDiskName(invalid_input6));
  EXPECT_FALSE(IsValidDiskName(invalid_input7));
  EXPECT_FALSE(IsValidDiskName(invalid_input8));

  std::string valid_input1 = "sda";
  std::string valid_input2 = "sdaaaa";
  std::string valid_input3 = "hdz";
  std::string valid_input4 = "mmcblk0";
  std::string valid_input5 = "mmcblk999";

  EXPECT_TRUE(IsValidDiskName(valid_input1));
  EXPECT_TRUE(IsValidDiskName(valid_input2));
  EXPECT_TRUE(IsValidDiskName(valid_input3));
  EXPECT_TRUE(IsValidDiskName(valid_input4));
  EXPECT_TRUE(IsValidDiskName(valid_input5));
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

}  // namespace debug
}  // namespace base
