// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_NAMESPACE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_NAMESPACE_H_

#include <string>
#include <vector>

#include "components/policy/policy_export.h"

namespace policy {

// Policies are namespaced by a (PolicyDomain, ID) pair. The meaning of the ID
// string depends on the domain; for example, if the PolicyDomain is
// "extensions" then the ID identifies the extension that the policies control.
enum POLICY_EXPORT PolicyDomain {
  // The component ID for chrome policies is always the empty string.
  POLICY_DOMAIN_CHROME,

  // The extensions policy domain is a work in progress. Included here for
  // tests.
  POLICY_DOMAIN_EXTENSIONS,

  // Must be the last entry.
  POLICY_DOMAIN_SIZE,
};

// Groups a policy domain and a component ID in a single object representing
// a policy namespace. Objects of this class can be used as keys in std::maps.
struct POLICY_EXPORT PolicyNamespace {
 public:
  PolicyNamespace();
  PolicyNamespace(PolicyDomain domain, const std::string& component_id);
  PolicyNamespace(const PolicyNamespace& other);
  ~PolicyNamespace();

  PolicyNamespace& operator=(const PolicyNamespace& other);
  bool operator<(const PolicyNamespace& other) const;
  bool operator==(const PolicyNamespace& other) const;
  bool operator!=(const PolicyNamespace& other) const;

  PolicyDomain domain;
  std::string component_id;
};

typedef std::vector<PolicyNamespace> PolicyNamespaceList;

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_NAMESPACE_H_
