// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guid.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
TEST(GUIDTest, GUIDGeneratesAllZeroes) {
  uint64 bytes[] = { 0, 0 };
  std::string clientid = guid::RandomDataToGUIDString(bytes);
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", clientid);
}

TEST(GUIDTest, GUIDGeneratesCorrectly) {
  uint64 bytes[] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL };
  std::string clientid = guid::RandomDataToGUIDString(bytes);
  EXPECT_EQ("01234567-89AB-CDEF-FEDC-BA9876543210", clientid);
}
#endif

TEST(GUIDTest, GUIDCorrectlyFormatted) {
  const int kIterations = 10;
  for (int it = 0; it < kIterations; ++it) {
    std::string guid = guid::GenerateGUID();
    EXPECT_EQ(36U, guid.length());
    std::string hexchars = "0123456789ABCDEF";
    for (uint32 i = 0; i < guid.length(); ++i) {
      char current = guid.at(i);
      if (i == 8 || i == 13 || i == 18 || i == 23) {
        EXPECT_EQ('-', current);
      } else {
        EXPECT_TRUE(std::string::npos != hexchars.find(current));
      }
    }
  }
}

TEST(GUIDTest, GUIDBasicUniqueness) {
  const int kIterations = 10;
  for (int it = 0; it < kIterations; ++it) {
    std::string guid1 = guid::GenerateGUID();
    std::string guid2 = guid::GenerateGUID();
    EXPECT_EQ(36U, guid1.length());
    EXPECT_EQ(36U, guid2.length());
    EXPECT_NE(guid1, guid2);
  }
}
