// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IP_MAPPING_RULES_H_
#define NET_BASE_IP_MAPPING_RULES_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/net_export.h"

namespace net {

class AddressList;

// This class contains a list of rules, that can be used to process (Rewrite) a
// set of DNS resolutions (IP addresses) in accordance with those rules.
// Currently the only rules supported use a pattern match against some given IP
// addresses, and may, if there is a match, place one new IP address at the
// start of the rewritten address list (re: the "PREFACE" directive).  This
// supports a common use case where the matching detects an address in a given
// edge network server group, and the inserted address points to a server that
// is expected to handle all requests that would otherwise have gone to the
// given group of servers.
class NET_EXPORT_PRIVATE IPMappingRules {
 public:
  IPMappingRules();
  ~IPMappingRules();

  // Modifies |*addresses| based on the current rules. Returns true if
  // |addresses| was modified, false otherwise.
  bool RewriteAddresses(AddressList* addresses) const;

  // Adds a rule to this mapper.
  // Rules are evaluated against an address list until a first matching rule is
  // found that applies, causing a modification of the address list.
  // Each rule consists of one or more of the following pattern and action
  // directives.
  //
  //   RULE: CHANGE_DIRECTIVE | EMPTY
  //   CHANGE_DIRECTIVE: "PREFACE" SPACE IP_PATTERN SPACE IP_LITERAL
  //   SPACE: " "
  //   EMPTY:
  //
  //   IP_LITERAL: IP_V6_LITERAL | IP_V4_LITERAL
  //
  //   IP_V4_LITERAL: OCTET "." OCTET "." OCTET "." OCTET
  //   OCTET: decimal number in range 0 ... 255
  //
  //   IP_V6_LITERAL:  HEX_COMPONENT  | IP_V6_LITERAL ":" HEX_COMPONENT
  //   HEX_COMPONENT: hexadecimal values with no more than 4 hex digits
  //
  //   IP_PATTERN: IP_V6_PATTERN | IP_V4_PATTERN
  //
  //   IP_V6_PATTERN: HEX_PATTERN | IP_V6_PATTERN ":" HEX_PATTERN
  //   HEX_PATTERN:  HEX_COMPONENT | "[" HEX_GROUP_PATTERN "]" | "*"
  //   HEX_GROUP_PATTERN: HEX_RANGE | HEX_GROUP_PATTERN "," HEX_RANGE
  //   HEX_RANGE: HEX_COMPONENT |  HEX_COMPONENT "-" HEX_COMPONENT
  //
  //   IP_V4_PATTERN: OCTET_PATTERN "." OCTET_PATTERN "." OCTET_PATTERN
  //                  "." OCTET_PATTERN
  //   OCTET_PATTERN:  OCTET | "[" OCTET_GROUP_PATTERN "]" | "*"
  //   OCTET_GROUP_PATTERN: OCTET_RANGE | OCTET_GROUP_PATTERN "," OCTET_RANGE
  //   OCTET_RANGE: OCTET | OCTET "-" OCTET
  //
  // All literals are required to have all their components specified (4
  // components for IPv4, and 8 for IPv6).  Specifically, there is currently no
  // support for elided compenents in an IPv6 address (e.g., "::").
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(const std::string& rule_string);

  // Sets the rules from a semicolon separated list of rules.
  // Returns true if all the rules were successfully parsed and added.
  bool SetRulesFromString(const std::string& rules_string);

 private:
  class Rule;
  typedef std::vector<Rule*> RuleList;

  // We own all rules in this list, and are responsible for their destruction.
  RuleList rules_;

  DISALLOW_COPY_AND_ASSIGN(IPMappingRules);
};

}  // namespace net

#endif  // NET_BASE_IP_MAPPING_RULES_H_
