// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_control_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class NumberInfoTest : public testing::Test {
 public:
  NumberInfoTest() { }
  virtual ~NumberInfoTest() { }

  typedef GpuControlList::FloatInfo FloatInfo;
  typedef GpuControlList::IntInfo IntInfo;
};

TEST_F(NumberInfoTest, ValidFloatInfo) {
  const std::string op[] = {
    "=",
    "<",
    "<=",
    ">",
    ">=",
    "any",
    "between"
  };
  for (size_t i = 0; i < arraysize(op); ++i) {
    std::string value1;
    std::string value2;
    if (op[i] != "any")
      value1 = "3.14";
    if (op[i] == "between")
      value2 = "4.21";
    FloatInfo info(op[i], value1, value2);
    EXPECT_TRUE(info.IsValid());
  }

  const std::string value[] = {
    "1.0E12",
    "1.0e12",
    "2013",
    "1.0e-12",
    "2.1400",
    "-2.14",
  };
  for (size_t i = 0; i < arraysize(value); ++i) {
    FloatInfo info("=", value[i], "");
    EXPECT_TRUE(info.IsValid());
  }
}

TEST_F(NumberInfoTest, InvalidFloatInfo) {
  const std::string op[] = {
    "=",
    "<",
    "<=",
    ">",
    ">=",
  };
  for (size_t i = 0; i < arraysize(op); ++i) {
    FloatInfo info(op[i], "", "");
    EXPECT_FALSE(info.IsValid());
  }
  {
    FloatInfo info("between", "3.14", "");
    EXPECT_FALSE(info.IsValid());
  }
  const std::string value[] = {
    "1.0 E12",
    "1.0e 12",
    " 2013",
    "2013 ",
    "- 2.14",
  };
  for (size_t i = 0; i < arraysize(value); ++i) {
    FloatInfo info("=", value[i], "");
    EXPECT_FALSE(info.IsValid());
  }
}

TEST_F(NumberInfoTest, FloatComparison) {
  {
    FloatInfo info("=", "3.14", "");
    EXPECT_TRUE(info.Contains(3.14f));
    EXPECT_TRUE(info.Contains(3.1400f));
    EXPECT_FALSE(info.Contains(3.1f));
    EXPECT_FALSE(info.Contains(3));
  }
  {
    FloatInfo info(">", "3.14", "");
    EXPECT_FALSE(info.Contains(3.14f));
    EXPECT_TRUE(info.Contains(3.141f));
    EXPECT_FALSE(info.Contains(3.1f));
  }
  {
    FloatInfo info("<=", "3.14", "");
    EXPECT_TRUE(info.Contains(3.14f));
    EXPECT_FALSE(info.Contains(3.141f));
    EXPECT_TRUE(info.Contains(3.1f));
  }
  {
    FloatInfo info("any", "", "");
    EXPECT_TRUE(info.Contains(3.14f));
  }
  {
    FloatInfo info("between", "3.14", "5.4");
    EXPECT_TRUE(info.Contains(3.14f));
    EXPECT_TRUE(info.Contains(5.4f));
    EXPECT_TRUE(info.Contains(4));
    EXPECT_FALSE(info.Contains(5.6f));
    EXPECT_FALSE(info.Contains(3.12f));
  }
}

TEST_F(NumberInfoTest, ValidIntInfo) {
  const std::string op[] = {
    "=",
    "<",
    "<=",
    ">",
    ">=",
    "any",
    "between"
  };
  for (size_t i = 0; i < arraysize(op); ++i) {
    std::string value1;
    std::string value2;
    if (op[i] != "any")
      value1 = "3";
    if (op[i] == "between")
      value2 = "9";
    IntInfo info(op[i], value1, value2);
    EXPECT_TRUE(info.IsValid());
  }

  const std::string value[] = {
    "12",
    "-12",
  };
  for (size_t i = 0; i < arraysize(value); ++i) {
    IntInfo info("=", value[i], "");
    EXPECT_TRUE(info.IsValid());
  }
}

TEST_F(NumberInfoTest, InvalidIntInfo) {
  const std::string op[] = {
    "=",
    "<",
    "<=",
    ">",
    ">=",
  };
  for (size_t i = 0; i < arraysize(op); ++i) {
    IntInfo info(op[i], "", "");
    EXPECT_FALSE(info.IsValid());
  }
  {
    IntInfo info("between", "3", "");
    EXPECT_FALSE(info.IsValid());
  }
  const std::string value[] = {
    " 12",
    "12 ",
    "- 12",
    " -12",
    "3.14"
  };
  for (size_t i = 0; i < arraysize(value); ++i) {
    IntInfo info("=", value[i], "");
    EXPECT_FALSE(info.IsValid());
  }
}

TEST_F(NumberInfoTest, IntComparison) {
  {
    IntInfo info("=", "3", "");
    EXPECT_TRUE(info.Contains(3));
    EXPECT_FALSE(info.Contains(4));
  }
  {
    IntInfo info(">", "3", "");
    EXPECT_FALSE(info.Contains(2));
    EXPECT_FALSE(info.Contains(3));
    EXPECT_TRUE(info.Contains(4));
  }
  {
    IntInfo info("<=", "3", "");
    EXPECT_TRUE(info.Contains(2));
    EXPECT_TRUE(info.Contains(3));
    EXPECT_FALSE(info.Contains(4));
  }
  {
    IntInfo info("any", "", "");
    EXPECT_TRUE(info.Contains(3));
  }
  {
    IntInfo info("between", "3", "5");
    EXPECT_TRUE(info.Contains(3));
    EXPECT_TRUE(info.Contains(5));
    EXPECT_TRUE(info.Contains(4));
    EXPECT_FALSE(info.Contains(6));
    EXPECT_FALSE(info.Contains(2));
  }
}

}  // namespace content

