// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_test_utils.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_bundle.h"

namespace policy {

bool PolicyServiceIsEmpty(const PolicyService* service) {
  const PolicyMap& map = service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  if (!map.empty()) {
    base::DictionaryValue dict;
    for (PolicyMap::const_iterator it = map.begin(); it != map.end(); ++it)
      dict.SetWithoutPathExpansion(it->first, it->second.value->DeepCopy());
    LOG(WARNING) << "There are pre-existing policies in this machine: " << dict;
  }
  return map.empty();
}

}  // namespace policy

std::ostream& operator<<(std::ostream& os,
                         const policy::PolicyBundle& bundle) {
  os << "{" << std::endl;
  for (policy::PolicyBundle::const_iterator iter = bundle.begin();
       iter != bundle.end(); ++iter) {
    os << "  \"" << iter->first << "\": " << *iter->second << "," << std::endl;
  }
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, policy::PolicyScope scope) {
  switch (scope) {
    case policy::POLICY_SCOPE_USER: {
      os << "POLICY_SCOPE_USER";
      break;
    }
    case policy::POLICY_SCOPE_MACHINE: {
      os << "POLICY_SCOPE_MACHINE";
      break;
    }
    default: {
      os << "POLICY_SCOPE_UNKNOWN(" << int(scope) << ")";
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, policy::PolicyLevel level) {
  switch (level) {
    case policy::POLICY_LEVEL_RECOMMENDED: {
      os << "POLICY_LEVEL_RECOMMENDED";
      break;
    }
    case policy::POLICY_LEVEL_MANDATORY: {
      os << "POLICY_LEVEL_MANDATORY";
      break;
    }
    default: {
      os << "POLICY_LEVEL_UNKNOWN(" << int(level) << ")";
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, policy::PolicyDomain domain) {
  switch (domain) {
    case policy::POLICY_DOMAIN_CHROME: {
      os << "POLICY_DOMAIN_CHROME";
      break;
    }
    case policy::POLICY_DOMAIN_EXTENSIONS: {
      os << "POLICY_DOMAIN_EXTENSIONS";
      break;
    }
    default: {
      os << "POLICY_DOMAIN_UNKNOWN(" << int(domain) << ")";
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const policy::PolicyMap& policies) {
  os << "{" << std::endl;
  for (policy::PolicyMap::const_iterator iter = policies.begin();
       iter != policies.end(); ++iter) {
    os << "  \"" << iter->first << "\": " << iter->second << "," << std::endl;
  }
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, const policy::PolicyMap::Entry& e) {
  std::string value;
  base::JSONWriter::WriteWithOptions(e.value,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &value);
  os << "{" << std::endl
     << "  \"level\": " << e.level << "," << std::endl
     << "  \"scope\": " << e.scope << "," << std::endl
     << "  \"value\": " << value
     << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, const policy::PolicyNamespace& ns) {
  os << ns.domain << "/" << ns.component_id;
  return os;
}
