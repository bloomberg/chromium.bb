// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_type_pattern.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class NetworkTypePatternTest : public testing::Test {
 public:
  NetworkTypePatternTest()
      : cellular_(NetworkTypePattern::Cellular()),
        default_(NetworkTypePattern::Default()),
        ethernet_(NetworkTypePattern::Ethernet()),
        mobile_(NetworkTypePattern::Mobile()),
        non_virtual_(NetworkTypePattern::NonVirtual()),
        wimax_(NetworkTypePattern::Wimax()),
        wireless_(NetworkTypePattern::Wireless()) {}

  bool MatchesPattern(const NetworkTypePattern& a,
                      const NetworkTypePattern& b) {
    // Verify that NetworkTypePattern::MatchesPattern is symmetric.
    EXPECT_TRUE(a.MatchesPattern(b) == b.MatchesPattern(a));
    return a.MatchesPattern(b);
  }

 protected:
  const NetworkTypePattern cellular_;
  const NetworkTypePattern default_;
  const NetworkTypePattern ethernet_;
  const NetworkTypePattern mobile_;
  const NetworkTypePattern non_virtual_;
  const NetworkTypePattern wimax_;
  const NetworkTypePattern wireless_;
};

}  // namespace

TEST_F(NetworkTypePatternTest, MatchesType) {
  EXPECT_TRUE(mobile_.MatchesType(shill::kTypeCellular));
  EXPECT_TRUE(mobile_.MatchesType(shill::kTypeWimax));
  EXPECT_FALSE(mobile_.MatchesType(shill::kTypeWifi));

  EXPECT_TRUE(wireless_.MatchesType(shill::kTypeWifi));
  EXPECT_TRUE(wireless_.MatchesType(shill::kTypeCellular));
  EXPECT_TRUE(wireless_.MatchesType(shill::kTypeWimax));
  EXPECT_FALSE(wireless_.MatchesType(shill::kTypeEthernet));
}

TEST_F(NetworkTypePatternTest, MatchesPattern) {
  // Each pair of {Mobile, Wireless, Cellular} is matching. Matching is
  // reflexive and symmetric (checked in MatchesPattern).
  EXPECT_TRUE(MatchesPattern(mobile_, mobile_));
  EXPECT_TRUE(MatchesPattern(wireless_, wireless_));
  EXPECT_TRUE(MatchesPattern(cellular_, cellular_));

  EXPECT_TRUE(MatchesPattern(mobile_, wireless_));
  EXPECT_TRUE(MatchesPattern(mobile_, cellular_));
  EXPECT_TRUE(MatchesPattern(wireless_, cellular_));

  // Cellular matches NonVirtual. NonVirtual matches Ethernet. But Cellular
  // doesn't match Ethernet.
  EXPECT_TRUE(MatchesPattern(cellular_, non_virtual_));
  EXPECT_TRUE(MatchesPattern(non_virtual_, ethernet_));
  EXPECT_FALSE(MatchesPattern(cellular_, ethernet_));

  // Default matches anything.
  EXPECT_TRUE(MatchesPattern(default_, default_));
  EXPECT_TRUE(MatchesPattern(default_, non_virtual_));
  EXPECT_TRUE(MatchesPattern(default_, cellular_));
}

TEST_F(NetworkTypePatternTest, Equals) {
  EXPECT_TRUE(mobile_.Equals(mobile_));
  EXPECT_FALSE(mobile_.Equals(cellular_));
  EXPECT_FALSE(cellular_.Equals(mobile_));
}

TEST_F(NetworkTypePatternTest, Primitive) {
  const NetworkTypePattern primitive_cellular =
      NetworkTypePattern::Primitive(shill::kTypeCellular);
  EXPECT_TRUE(cellular_.Equals(primitive_cellular));
  EXPECT_TRUE(primitive_cellular.Equals(cellular_));

  const NetworkTypePattern primitive_wimax =
      NetworkTypePattern::Primitive(shill::kTypeWimax);
  EXPECT_TRUE(wimax_.Equals(primitive_wimax));
  EXPECT_TRUE(primitive_wimax.Equals(wimax_));
}

TEST_F(NetworkTypePatternTest, ToDebugString) {
  EXPECT_EQ(default_.ToDebugString(), "PatternDefault");
  EXPECT_EQ(mobile_.ToDebugString(), "PatternMobile");
  EXPECT_EQ(cellular_.ToDebugString(), "cellular");
}

}  // namespace chromeos
