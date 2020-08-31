// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_VALUE_VALIDATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_VALUE_VALIDATOR_H_

#include <string>

#include "base/macros.h"

namespace policy {

struct ValueValidationIssue {
  enum Severity { kWarning, kError };

  std::string policy_name;
  Severity severity = kWarning;
  std::string message;

  bool operator==(ValueValidationIssue const& rhs) const {
    return policy_name == rhs.policy_name && severity == rhs.severity &&
           message == rhs.message;
  }
};

template <typename PayloadProto>
class PolicyValueValidator {
 public:
  PolicyValueValidator() {}
  virtual ~PolicyValueValidator() {}

  // Returns false if the value validation failed with errors.
  virtual bool ValidateValues(
      const PayloadProto& policy_payload,
      std::vector<ValueValidationIssue>* out_validation_issues) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyValueValidator);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_VALUE_VALIDATOR_H_