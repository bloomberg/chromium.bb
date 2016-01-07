// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_ANONYMIZER_TOOL_H_
#define COMPONENTS_FEEDBACK_ANONYMIZER_TOOL_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>

namespace feedback {

class AnonymizerTool {
 public:
  AnonymizerTool();
  ~AnonymizerTool();

  // Returns an anonymized version of |input|. PII-sensitive data (such as MAC
  // addresses) in |input| is replaced with unique identifiers.
  std::string Anonymize(const std::string& input);

 private:
  friend class AnonymizerToolTest;

  std::string AnonymizeMACAddresses(const std::string& input);
  std::string AnonymizeCustomPatterns(std::string input);
  static std::string AnonymizeCustomPattern(
      const std::string& input,
      const std::string& pattern,
      std::map<std::string, std::string>* identifier_space);

  // Map of MAC addresses discovered in anonymized strings to anonymized
  // representations. 11:22:33:44:55:66 gets anonymized to 11:22:33:00:00:01,
  // where the first three bytes represent the manufacturer. The last three
  // bytes are used to distinguish different MAC addresses and are incremented
  // for each newly discovered MAC address.
  std::map<std::string, std::string> mac_addresses_;

  // Like mac addresses, identifiers in custom patterns are anonymized.
  // custom_patterns_[i] contains a map of original identifier to anonymized
  // identifier for custom pattern number i.
  std::vector<std::map<std::string, std::string>> custom_patterns_;

  DISALLOW_COPY_AND_ASSIGN(AnonymizerTool);
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_ANONYMIZER_TOOL_H_
