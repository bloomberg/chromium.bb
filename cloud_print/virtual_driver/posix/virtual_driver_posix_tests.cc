// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/posix/printer_driver_util_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printer_driver_util {

void test_helper(const char* input, std::string expected_output) {
  std::string test;
  GetOptions(input, &test);
  EXPECT_EQ(expected_output, test);
}

TEST(PrintTicketTest, HandlesEmpty) {
  test_helper("", "{}");
}

TEST(PrintTicketTest, HandlesNull) {
  test_helper(NULL, "{}");
}

TEST(PrintTicketTest, HandlesOneOption) {
  test_helper("Resolution=500", "{\"Resolution\":\"500\"}");
}

TEST(PrintTicketTest, HandlesMutipleOptions) {
  test_helper("Resolution=500 DPI=1100",
              "{\"DPI\":\"1100\",\"Resolution\":\"500\"}");
}

TEST(PrintTicketTest, HandlesErrorInOptions) {
  test_helper("Resolution=500 Foo DPI=1100",
              "{\"DPI\":\"1100\",\"Resolution\":\"500\"}");
}

TEST(PrintTicketTest, HandlesMutipleSpaces) {
  test_helper("Resolution=500  DPI=1100",
              "{\"DPI\":\"1100\",\"Resolution\":\"500\"}");
}

TEST(PrintTicketTest, HandlesEscapedSpace) {
  test_helper("Job\\ Owner=First\\ Last",
              "{\"Job Owner\":\"First Last\"}");
}

TEST(PrintTicketTest, HandlesMultipleEscapedWords) {
  test_helper("Job\\ Owner\\ Name=First\\ Last",
              "{\"Job Owner Name\":\"First Last\"}");
}

TEST(PrintTicketTest, HandlesMultipleEscapedSpaces) {
  test_helper("Job\\ Owner\\ \\ Name=First\\ Last",
              "{\"Job Owner  Name\":\"First Last\"}");
}

TEST(PrintTicketTest, HandlesKeyEndsInEscapedSpace) {
    test_helper("Job\\ Owner\\ =First\\ Last",
                "{\"Job Owner \":\"First Last\"}");
}

TEST(PrintTicketTest, HandlesSlashAtEnd) {
  test_helper("Job\\ Owner=First\\ Last\\",
              "{\"Job Owner\":\"First Last\\\\\"}");
}


}  // namespace printer_driver_util

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

