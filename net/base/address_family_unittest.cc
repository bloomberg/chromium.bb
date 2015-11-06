// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_family.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(AddressFamilyTest, GetAddressFamily) {
  IPAddressNumber number;
  EXPECT_EQ(ADDRESS_FAMILY_UNSPECIFIED, GetAddressFamily(number));
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &number));
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, GetAddressFamily(number));
  EXPECT_TRUE(ParseIPLiteralToNumber("1:abcd::3:4:ff", &number));
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, GetAddressFamily(number));
}

}  // namespace
}  // namespace net
