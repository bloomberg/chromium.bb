// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_info.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

TEST(ScreenInfoTest, Compare) {
  const ScreenInfo screen1{"id1", "name1", 1, {{192, 168, 1, 10}, 12345}};
  const ScreenInfo screen2{"id2", "name2", 1, {{192, 168, 1, 11}, 12346}};
  const ScreenInfo screen1_alt_id{
      "id3", "name1", 1, {{192, 168, 1, 10}, 12345}};
  const ScreenInfo screen1_alt_name{
      "id1", "name2", 1, {{192, 168, 1, 10}, 12345}};
  const ScreenInfo screen1_alt_interface{
      "id1", "name1", 2, {{192, 168, 1, 10}, 12345}};
  const ScreenInfo screen1_alt_ip{
      "id3", "name1", 1, {{192, 168, 1, 12}, 12345}};
  const ScreenInfo screen1_alt_port{
      "id3", "name1", 1, {{192, 168, 1, 10}, 12645}};
  ScreenInfo screen1_ipv6{"id3", "name1", 1, {{}, 4321}};
  ASSERT_TRUE(IPAddress::Parse("::12:34", &screen1_ipv6.endpoint.address));

  EXPECT_EQ(screen1, screen1);
  EXPECT_EQ(screen2, screen2);
  EXPECT_NE(screen1, screen2);
  EXPECT_NE(screen2, screen1);

  EXPECT_NE(screen1, screen1_alt_id);
  EXPECT_NE(screen1, screen1_alt_name);
  EXPECT_NE(screen1, screen1_alt_interface);
  EXPECT_NE(screen1, screen1_alt_ip);
  EXPECT_NE(screen1, screen1_alt_port);
  EXPECT_NE(screen1, screen1_ipv6);
}

}  // namespace openscreen
