// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/test/webdriver/frame_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webdriver {

TEST(FramePathTest, Constructors) {
  EXPECT_EQ("", FramePath().value());
  EXPECT_EQ("frame", FramePath("frame").value());
  EXPECT_EQ("frame", FramePath(FramePath("frame")).value());
}

TEST(FramePathTest, Append) {
  EXPECT_EQ("frame", FramePath().Append("frame").value());
  EXPECT_EQ("frame1\nframe2", FramePath("frame1").Append("frame2").value());
}

TEST(FramePathTest, Parent) {
  FramePath path = FramePath("frame1").Append("frame2");
  EXPECT_EQ("frame1", path.Parent().value());
  EXPECT_EQ("", path.Parent().Parent().value());
  EXPECT_EQ("", path.Parent().Parent().Parent().value());
}

TEST(FramePathTest, BaseName) {
  FramePath path = FramePath("frame1").Append("frame2");
  EXPECT_EQ("frame2", path.BaseName().value());
  EXPECT_EQ("frame1", path.Parent().BaseName().value());
  EXPECT_EQ("", path.Parent().Parent().BaseName().value());
}

TEST(FramePathTest, GetComponents) {
  FramePath path = FramePath("frame1").Append("frame2");
  std::vector<std::string> components;
  path.GetComponents(&components);
  ASSERT_EQ(2u, components.size());
  EXPECT_EQ("frame1", components[0]);
  EXPECT_EQ("frame2", components[1]);

  components.clear();
  path.Parent().GetComponents(&components);
  ASSERT_EQ(1u, components.size());
  EXPECT_EQ("frame1", components[0]);

  components.clear();
  path.Parent().Parent().GetComponents(&components);
  EXPECT_EQ(0u, components.size());
}

TEST(FramePathTest, IsRootFrame) {
  EXPECT_TRUE(FramePath().IsRootFrame());
  EXPECT_FALSE(FramePath("frame").IsRootFrame());
}

TEST(FramePathTest, IsSubframe) {
  EXPECT_FALSE(FramePath().IsSubframe());
  EXPECT_TRUE(FramePath("frame").IsSubframe());
}

}  // namespace webdriver
