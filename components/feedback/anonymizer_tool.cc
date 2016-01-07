// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/anonymizer_tool.h"

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "third_party/re2/src/re2/re2.h"

using re2::RE2;

namespace feedback {

namespace {

// The |kCustomPatterns| array defines patterns to match and anonymize. Each
// pattern needs to define three capturing parentheses groups:
//
// - a group for the pattern before the identifier to be anonymized;
// - a group for the identifier to be anonymized;
// - a group for the pattern after the identifier to be anonymized.
//
// Every matched identifier (in the context of the whole pattern) is anonymized
// by replacing it with an incremental instance identifier. Every different
// pattern defines a separate instance identifier space. See the unit test for
// AnonymizerTool::AnonymizeCustomPattern for pattern anonymization examples.
//
// Useful regular expression syntax:
//
// +? is a non-greedy (lazy) +.
// \b matches a word boundary.
// (?i) turns on case insensitivy for the remainder of the regex.
// (?-s) turns off "dot matches newline" for the remainder of the regex.
// (?:regex) denotes non-capturing parentheses group.
const char* kCustomPatterns[] = {
    "(\\bCell ID: ')([0-9a-fA-F]+)(')",                  // ModemManager
    "(\\bLocation area code: ')([0-9a-fA-F]+)(')",       // ModemManager
    "(?i-s)(\\bssid[= ]')(.+)(')",                       // wpa_supplicant
    "(?-s)(\\bSSID - hexdump\\(len=[0-9]+\\): )(.+)()",  // wpa_supplicant
    "(?-s)(\\[SSID=)(.+?)(\\])",                         // shill
};

}  // namespace

AnonymizerTool::AnonymizerTool()
    : custom_patterns_(arraysize(kCustomPatterns)) {}

AnonymizerTool::~AnonymizerTool() {}

std::string AnonymizerTool::Anonymize(const std::string& input) {
  std::string anonymized = AnonymizeMACAddresses(input);
  anonymized = AnonymizeCustomPatterns(std::move(anonymized));
  return anonymized;
}

std::string AnonymizerTool::AnonymizeMACAddresses(const std::string& input) {
  // This regular expression finds the next MAC address. It splits the data into
  // a section preceding the MAC address, an OUI (Organizationally Unique
  // Identifier) part and a NIC (Network Interface Controller) specific part.

  RE2::Options options;
  // set_multiline of pcre is not supported by RE2, yet.
  options.set_dot_nl(true);  // Dot matches a new line.
  RE2 mac_re(
      "(.*?)("
      "[0-9a-fA-F][0-9a-fA-F]:"
      "[0-9a-fA-F][0-9a-fA-F]:"
      "[0-9a-fA-F][0-9a-fA-F]):("
      "[0-9a-fA-F][0-9a-fA-F]:"
      "[0-9a-fA-F][0-9a-fA-F]:"
      "[0-9a-fA-F][0-9a-fA-F])",
      options);

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  std::string pre_mac, oui, nic;
  while (re2::RE2::Consume(&text, mac_re, RE2::Arg(&pre_mac), RE2::Arg(&oui),
                           RE2::Arg(&nic))) {
    // Look up the MAC address in the hash.
    oui = base::ToLowerASCII(oui);
    nic = base::ToLowerASCII(nic);
    std::string mac = oui + ":" + nic;
    std::string replacement_mac = mac_addresses_[mac];
    if (replacement_mac.empty()) {
      // If not found, build up a replacement MAC address by generating a new
      // NIC part.
      int mac_id = mac_addresses_.size();
      replacement_mac = base::StringPrintf(
          "%s:%02x:%02x:%02x", oui.c_str(), (mac_id & 0x00ff0000) >> 16,
          (mac_id & 0x0000ff00) >> 8, (mac_id & 0x000000ff));
      mac_addresses_[mac] = replacement_mac;
    }

    result += pre_mac;
    result += replacement_mac;
  }

  text.AppendToString(&result);
  return result;
}

std::string AnonymizerTool::AnonymizeCustomPatterns(std::string input) {
  for (size_t i = 0; i < arraysize(kCustomPatterns); i++) {
    input =
        AnonymizeCustomPattern(input, kCustomPatterns[i], &custom_patterns_[i]);
  }
  return input;
}

// static
std::string AnonymizerTool::AnonymizeCustomPattern(
    const std::string& input,
    const std::string& pattern,
    std::map<std::string, std::string>* identifier_space) {
  RE2::Options options;
  // set_multiline of pcre is not supported by RE2, yet.
  options.set_dot_nl(true);  // Dot matches a new line.
  RE2 re("(.*?)" + pattern, options);
  DCHECK_EQ(4, re.NumberOfCapturingGroups());

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  std::string pre_match, pre_matched_id, matched_id, post_matched_id;
  while (RE2::Consume(&text, re, RE2::Arg(&pre_match),
                      RE2::Arg(&pre_matched_id), RE2::Arg(&matched_id),
                      RE2::Arg(&post_matched_id))) {
    std::string replacement_id = (*identifier_space)[matched_id];
    if (replacement_id.empty()) {
      replacement_id = base::IntToString(identifier_space->size());
      (*identifier_space)[matched_id] = replacement_id;
    }

    result += pre_match;
    result += pre_matched_id;
    result += replacement_id;
    result += post_matched_id;
  }
  text.AppendToString(&result);
  return result;
}

}  // namespace feedback
