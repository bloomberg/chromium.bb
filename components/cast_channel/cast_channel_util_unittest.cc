// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_channel_util.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_channel {

TEST(CastChannelUtilTest, IsValidCastIPAddress) {
  std::string private_ip_address_string = "192.168.0.1";
  net::IPAddress private_ip_address;
  ASSERT_TRUE(
      private_ip_address.AssignFromIPLiteral(private_ip_address_string));
  EXPECT_TRUE(IsValidCastIPAddress(private_ip_address));
  EXPECT_TRUE(IsValidCastIPAddressString(private_ip_address_string));

  std::string public_ip_address_string = "133.1.2.3";
  net::IPAddress public_ip_address;
  ASSERT_TRUE(public_ip_address.AssignFromIPLiteral(public_ip_address_string));
  EXPECT_FALSE(IsValidCastIPAddress(public_ip_address));
  EXPECT_FALSE(IsValidCastIPAddressString(public_ip_address_string));

  EXPECT_FALSE(IsValidCastIPAddressString("not a valid ip address"));
}

TEST(CastChannelUtilTest, IsValidCastIPAddressAllowAllIPs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCastAllowAllIPsFeature);

  std::string private_ip_address_string = "192.168.0.1";
  net::IPAddress private_ip_address;
  ASSERT_TRUE(
      private_ip_address.AssignFromIPLiteral(private_ip_address_string));
  EXPECT_TRUE(IsValidCastIPAddress(private_ip_address));
  EXPECT_TRUE(IsValidCastIPAddressString(private_ip_address_string));

  std::string public_ip_address_string = "133.1.2.3";
  net::IPAddress public_ip_address;
  ASSERT_TRUE(public_ip_address.AssignFromIPLiteral(public_ip_address_string));
  EXPECT_TRUE(IsValidCastIPAddress(public_ip_address));
  EXPECT_TRUE(IsValidCastIPAddressString(public_ip_address_string));

  EXPECT_FALSE(IsValidCastIPAddressString("not a valid ip address"));
}

}  // namespace cast_channel
