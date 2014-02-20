// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_mapping_rules.h"

#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(IPMappingRulesTest, EmptyRules) {
  AddressList addresses;
  // Null rule does nothing to null list.
  EXPECT_EQ(0u, addresses.size());
  IPMappingRules rules;
  EXPECT_FALSE(rules.RewriteAddresses(&addresses));
  EXPECT_EQ(0u, addresses.size());

  IPAddressNumber address;
  EXPECT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &address));
  int port = 80;
  IPEndPoint endpoint(address, port);
  addresses.push_back(endpoint);
  // Null rule does nothing to a simple list.
  ASSERT_EQ(1u, addresses.size());
  EXPECT_EQ(addresses[0], endpoint);
  EXPECT_FALSE(rules.RewriteAddresses(&addresses));
  ASSERT_EQ(1u, addresses.size());
  EXPECT_EQ(addresses[0], endpoint);

  EXPECT_TRUE(ParseIPLiteralToNumber("1:2:3:4:a:b:c:def", &address));
  IPEndPoint endpoint2(address, port);
  addresses.push_back(endpoint2);
  // Null rule does nothing to a simple list.
  ASSERT_EQ(2u, addresses.size());
  EXPECT_EQ(addresses[0], endpoint);
  EXPECT_EQ(addresses[1], endpoint2);
  EXPECT_FALSE(rules.RewriteAddresses(&addresses));
  ASSERT_EQ(2u, addresses.size());
  EXPECT_EQ(addresses[0], endpoint);
  EXPECT_EQ(addresses[1], endpoint2);
}

// The |inserted_ip| must have a short final component, so that we can just
// append a digit, and still be a valid (but different) ip address.
void MatchingInsertionHelper(const std::string& matchable_ip,
                             const std::string& matching_ip_pattern,
                             const std::string& inserted_ip,
                             bool should_match) {
  AddressList addresses;

  IPAddressNumber address;
  EXPECT_TRUE(ParseIPLiteralToNumber(matchable_ip, &address));
  int port = 80;
  IPEndPoint endpoint(address, port);
  addresses.push_back(endpoint);
  // Match the string with a precisely crafted pattern.
  std::string rule_text("Preface ");
  rule_text += matching_ip_pattern;
  rule_text += " ";
  rule_text += inserted_ip;
  IPMappingRules rules;
  EXPECT_TRUE(rules.AddRuleFromString(rule_text));

  ASSERT_EQ(1u, addresses.size());
  EXPECT_EQ(addresses[0], endpoint);
  EXPECT_EQ(should_match, rules.RewriteAddresses(&addresses));
  if (!should_match) {
    ASSERT_EQ(1u, addresses.size());
    EXPECT_EQ(addresses[0], endpoint);
    return;  // Don't bother with the rest of the test.
  }

  ASSERT_EQ(2u, addresses.size());
  EXPECT_EQ(addresses[1], endpoint);
  IPAddressNumber inserted_ip_adress_number;
  EXPECT_TRUE(ParseIPLiteralToNumber(inserted_ip, &inserted_ip_adress_number));
  EXPECT_EQ(inserted_ip_adress_number, addresses[0].address());

  // Now we have two addresses.  We'll use new rules, to match the second
  // address (the original address shifted over) in the list of addresses.
  rule_text = "Preface ";  // Build up a new rule.
  rule_text += matching_ip_pattern;
  rule_text += " ";
  // Change the inserted IP mildly.
  std::string different_inserted_ip = inserted_ip + "3";
  rule_text += different_inserted_ip;
  // If we don't overwrite the old rule, it will fire first!
  EXPECT_TRUE(rules.SetRulesFromString(rule_text));  // Overwrite old rules.

  ASSERT_EQ(2u, addresses.size());
  EXPECT_EQ(addresses[1], endpoint);
  EXPECT_TRUE(rules.RewriteAddresses(&addresses));
  ASSERT_EQ(3u, addresses.size());
  EXPECT_EQ(addresses[2], endpoint);
  EXPECT_TRUE(ParseIPLiteralToNumber(different_inserted_ip,
                                     &inserted_ip_adress_number));
  EXPECT_EQ(inserted_ip_adress_number, addresses[0].address());
}

TEST(IPMappingRulesTest, SimpleMatchingInsertionIPV4) {
  std::string matchable_ip("1.2.3.4");
  std::string matching_ip_pattern(matchable_ip);
  std::string new_preface_ip("7.8.9.24");
  bool should_match = true;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, SimpleMatchingInsertionIPV6) {
  std::string matchable_ip("1:2:3:4:5:6:7:8");
  std::string matching_ip_pattern(matchable_ip);
  std::string new_preface_ip("A:B:C:D:1:2:3:4");
  bool should_match = true;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, SimpleMissatchingInsertionIPV4) {
  std::string matchable_ip("1.2.3.4");
  std::string matching_ip_pattern(matchable_ip + "7");
  std::string new_preface_ip("7.8.9.24");
  bool should_match = false;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, SimpleMisatchingInsertionIPV6) {
  std::string matchable_ip("1:2:3:4:5:6:7:8");
  std::string matching_ip_pattern(matchable_ip + "e");
  std::string new_preface_ip("A:B:C:D:1:2:3:4");
  bool should_match = false;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, AlternativeMatchingInsertionIPV4) {
  std::string matchable_ip("1.2.3.4");
  std::string matching_ip_pattern("1.2.3.[2,4,6]");
  std::string new_preface_ip("7.8.9.24");
  bool should_match = true;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, AlternativeMatchingInsertionIPV6) {
  std::string matchable_ip("1:2:3:4:5:6:7:abcd");
  std::string matching_ip_pattern("1:2:3:4:5:6:7:[abc2,abc9,abcd,cdef]");
  std::string new_preface_ip("A:B:C:D:1:2:3:4");
  bool should_match = true;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, RangeMatchingInsertionIPV4) {
  std::string matchable_ip("1.2.3.4");
  std::string matching_ip_pattern("1.2.3.[2-6,22]");
  std::string new_preface_ip("7.8.9.24");
  bool should_match = true;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, RandgeMatchingInsertionIPV6) {
  std::string matchable_ip("1:2:3:4:5:6:7:abcd");
  // Note: This test can detect confusion over high vs low-order bytes in an
  // IPv6 component.  If the code confused them, then this range would not
  // include the matchable_ip.
  std::string matching_ip_pattern("1:2:3:4:5:6:7:[abc2-cdc2]");
  std::string new_preface_ip("A:B:C:D:1:2:3:4");
  bool should_match = true;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, WildMatchingInsertionIPV4) {
  std::string matchable_ip("1.2.3.4");
  std::string matching_ip_pattern("1.2.3.*");
  std::string new_preface_ip("7.8.9.24");
  bool should_match = true;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, WildMatchingInsertionIPV6) {
  std::string matchable_ip("1:2:3:4:5:6:7:abcd");
  // Note: This test can detect confusion over high vs low-order bytes in an
  // IPv6 component.  If the code confused them, then this range would not
  // include the matchable_ip.
  std::string matching_ip_pattern("1:2:3:4:5:6:7:*");
  std::string new_preface_ip("A:B:C:D:1:2:3:4");
  bool should_match = true;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, WildNotMatchingInsertionIPV4) {
  std::string matchable_ip("1.2.3.4");
  std::string matching_ip_pattern("*.200.*.*");
  std::string new_preface_ip("7.8.9.24");
  bool should_match = false;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, WildNotMatchingInsertionIPV6) {
  std::string matchable_ip("1:2:3:4:5:6:7:8");
  // Note: This test can detect confusion over high vs low-order bytes in an
  // IPv6 component.  If the code confused them, then this range would not
  // include the matchable_ip.
  std::string matching_ip_pattern("*:*:37af:*:*:*:*:*");
  std::string new_preface_ip("A:B:C:D:1:2:3:4");
  bool should_match = false;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

TEST(IPMappingRulesTest, IPv4NotMatchForIPV6) {
  std::string matchable_ip("1:2:3:4:5:6:7:abcd");
  // Note: This test can detect confusion over high vs low-order bytes in an
  // IPv6 component.  If the code confused them, then this range would not
  // include the matchable_ip.
  std::string matching_ip_pattern("1.2.3.4");
  std::string new_preface_ip("10.20.30.40");  // Compatible with pattern.
  bool should_match = false;
  MatchingInsertionHelper(matchable_ip, matching_ip_pattern, new_preface_ip,
                          should_match);
}

// Parsing bad rules should silently discard the rule (and never crash).
TEST(IPMappingRulesTest, ParseInvalidRules) {
  IPMappingRules rules;

  EXPECT_FALSE(rules.AddRuleFromString("Too short"));
  EXPECT_FALSE(rules.AddRuleFromString("Preface much too long"));
  EXPECT_FALSE(rules.AddRuleFromString("PrefaceNotSpelled 1.2.3.4 1.2.3.4"));

  // Ipv4 problems
  EXPECT_FALSE(rules.AddRuleFromString("Preface 1.2.a.4 1.2.3.4"));
  EXPECT_FALSE(rules.AddRuleFromString("Preface 1.2.3.4.5 1.2.3.4"));
  EXPECT_FALSE(rules.AddRuleFromString("Preface 1.2.3.4 1.2.3.4.5"));
  EXPECT_FALSE(rules.AddRuleFromString("Preface 1.2.3.4 1.2.3.4-5"));
  EXPECT_FALSE(rules.AddRuleFromString("Preface 1.2.3.4-5-6 1.2.3.4"));

  // IPv6 problems
  EXPECT_FALSE(rules.AddRuleFromString(
      "Preface 1:2:3:4:5:6:7:g 1:2:3:4:5:6:7:8"));
  EXPECT_FALSE(rules.AddRuleFromString(
      "Preface 1:2:3:4:5:6:7:8 1:g:3:4:5:6:7:8"));
  EXPECT_FALSE(rules.AddRuleFromString(
      "Preface 1:2:3:4:5:6:7:8:9 1:2:3:4:5:6:7:8"));
  EXPECT_FALSE(rules.AddRuleFromString(
      "Preface 1:2:3:4:5:6:7:8 1:2:3:4:5:6:7:8:0"));
  EXPECT_FALSE(rules.AddRuleFromString(
      "Preface 1:2:3:4:5:6:7:8 1:2:3:4:5:6:7:8-A"));
  EXPECT_FALSE(rules.AddRuleFromString(
      "Preface 1:2:3:4:5:6:7:8-A-B 1:2:3:4:5:6:7:8"));

  // Don't mix ipv4 and ipv6.
  EXPECT_FALSE(rules.AddRuleFromString("Preface 1.2.3.4 1:2:3:4:5:6:7:8"));

  EXPECT_FALSE(rules.SetRulesFromString("Preface 1.2.3.4 5.6.7.8; bad"));
}


TEST(IPMappingRulesTest, FirstRuleToMatch) {
  std::string first_replacement("1.1.1.1");
  IPAddressNumber first_replacement_address;
  EXPECT_TRUE(ParseIPLiteralToNumber(first_replacement,
              &first_replacement_address));

  std::string second_replacement("2.2.2.2");
  IPAddressNumber second_replacement_address;
  EXPECT_TRUE(ParseIPLiteralToNumber(second_replacement,
              &second_replacement_address));

  IPMappingRules rules;
  EXPECT_TRUE(
      rules.SetRulesFromString("Preface *.*.*.* " + first_replacement +
                               ";Preface *.*.*.* " + second_replacement));

  IPAddressNumber address;
  EXPECT_TRUE(ParseIPLiteralToNumber("7.7.7.7", &address));
  int port = 80;
  IPEndPoint endpoint(address, port);
  AddressList addresses;
  addresses.push_back(endpoint);

  ASSERT_EQ(1u, addresses.size());
  EXPECT_EQ(addresses[0], endpoint);
  EXPECT_TRUE(rules.RewriteAddresses(&addresses));
  ASSERT_EQ(2u, addresses.size());
  EXPECT_EQ(addresses[1], endpoint);
  // The last rule added  to the list has the highest priority (overriding
  // previous rules, and matching first).
  EXPECT_NE(addresses[0].address(), first_replacement_address);
  EXPECT_EQ(addresses[0].address(), second_replacement_address);
}

}  // namespace

}  // namespace net
