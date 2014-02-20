// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_mapping_rules.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/base/address_list.h"
#include "net/base/ip_pattern.h"

namespace net {

class IPMappingRules::Rule {
 public:
  Rule(const IPAddressNumber& address, IPPattern* pattern)
      : pattern_(pattern),
        replacement_address_(address) {
  }

  bool Match(const IPAddressNumber& address) const {
    return pattern_->Match(address);
  }

  const IPAddressNumber& replacement_address() const {
    return replacement_address_;
  }

 private:
  scoped_ptr<IPPattern> pattern_;        // The pattern we can match.
  IPAddressNumber replacement_address_;  // The replacement address we provide.
  DISALLOW_COPY_AND_ASSIGN(Rule);
};

IPMappingRules::IPMappingRules() {}

IPMappingRules::~IPMappingRules() {
  STLDeleteElements(&rules_);
}

bool IPMappingRules::RewriteAddresses(AddressList* addresses) const {
  // Last rule pushed into the list has the highest priority, and is tested
  // first.
  for (RuleList::const_reverse_iterator rule_it = rules_.rbegin();
       rule_it != rules_.rend(); ++rule_it) {
    for (AddressList::iterator address_it = addresses->begin();
        address_it != addresses->end(); ++address_it) {
      if (!(*rule_it)->Match(address_it->address()))
        continue;
      // We have a match.
      int port = address_it->port();
      addresses->insert(addresses->begin(),
                        IPEndPoint((*rule_it)->replacement_address(), port));
      return true;
    }
  }
  return false;
}

bool IPMappingRules::AddRuleFromString(const std::string& rule_string) {
  std::string trimmed;
  TrimWhitespaceASCII(rule_string, TRIM_ALL, &trimmed);
  if (trimmed.empty())
    return true;  // Allow empty rules.

  std::vector<std::string> parts;
  base::SplitString(trimmed, ' ', &parts);

  if (parts.size() != 3) {
    DVLOG(1) << "Rule has wrong number of parts: " << rule_string;
    return false;
  }

  if (!LowerCaseEqualsASCII(parts[0], "preface")) {
    DVLOG(1) << "Unknown directive: " << rule_string;
    return false;
  }

  scoped_ptr<IPPattern> pattern(new IPPattern);
  if (!pattern->ParsePattern(parts[1])) {
    DVLOG(1) << "Unacceptable pattern: " << rule_string;
    return false;
  }

  IPAddressNumber address;
  if (!ParseIPLiteralToNumber(parts[2], &address)) {
    DVLOG(1) << "Invalid replacement address: " << rule_string;
    return false;
  }

  if (pattern->is_ipv4() != (address.size() == kIPv4AddressSize)) {
    DVLOG(1) << "Mixture of IPv4 and IPv6: " << rule_string;
    return false;
  }
  rules_.push_back(new Rule(address, pattern.release()));
  return true;
}

bool IPMappingRules::SetRulesFromString(const std::string& rules_string) {
  STLDeleteElements(&rules_);

  base::StringTokenizer rules(rules_string, ";");
  while (rules.GetNext()) {
    if (!AddRuleFromString(rules.token())) {
      DVLOG(1) << "Failed parsing rule: " << rules.token();
      STLDeleteElements(&rules_);
      return false;
    }
  }
  return true;
}

}  // namespace net
