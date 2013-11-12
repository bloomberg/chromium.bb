// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_TEST_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_TEST_POLICY_TEST_UTILS_H_

#include <map>
#include <ostream>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/policy_types.h"
#include "policy/policy_constants.h"

namespace policy {

class PolicyBundle;
struct PolicyNamespace;

// A mapping of policy names to PolicyDetails that can be used to set the
// PolicyDetails for test policies.
class PolicyDetailsMap {
 public:
  PolicyDetailsMap();
  ~PolicyDetailsMap();

  // The returned callback's lifetime is tied to |this| object.
  GetChromePolicyDetailsCallback GetCallback() const;

  // Does not take ownership of |details|.
  void SetDetails(const std::string& policy, const PolicyDetails* details);

 private:
  typedef std::map<std::string, const PolicyDetails*> PolicyDetailsMapping;

  const PolicyDetails* Lookup(const std::string& policy) const;

  PolicyDetailsMapping map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyDetailsMap);
};

// Returns true if |service| is not serving any policies. Otherwise logs the
// current policies and returns false.
bool PolicyServiceIsEmpty(const PolicyService* service);

}  // namespace policy

std::ostream& operator<<(std::ostream& os, const policy::PolicyBundle& bundle);
std::ostream& operator<<(std::ostream& os, policy::PolicyScope scope);
std::ostream& operator<<(std::ostream& os, policy::PolicyLevel level);
std::ostream& operator<<(std::ostream& os, policy::PolicyDomain domain);
std::ostream& operator<<(std::ostream& os, const policy::PolicyMap& policies);
std::ostream& operator<<(std::ostream& os, const policy::PolicyMap::Entry& e);
std::ostream& operator<<(std::ostream& os, const policy::PolicyNamespace& ns);

#endif  // CHROME_BROWSER_POLICY_TEST_POLICY_TEST_UTILS_H_
