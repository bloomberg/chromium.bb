// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_MAP_H_
#define CHROME_BROWSER_POLICY_POLICY_MAP_H_

#include <map>

#include "base/values.h"
#include "policy/configuration_policy_type.h"

namespace policy {

// Wrapper class around a std::map<ConfigurationPolicyType, Value*> that
// properly cleans up after itself when going out of scope.
// Exposes interesting methods of the underlying std::map.
class PolicyMap {
 public:
  typedef std::map<ConfigurationPolicyType, Value*> PolicyMapType;
  typedef PolicyMapType::const_iterator const_iterator;

  PolicyMap();
  virtual ~PolicyMap();

  // Returns a weak reference to the value currently stored for key |policy|.
  // Ownership is retained by PolicyMap; callers should use Value::DeepCopy
  // if they need a copy that they own themselves.
  // Returns NULL if the map does not contain a value for |policy|.
  const Value* Get(ConfigurationPolicyType policy) const;
  // Takes ownership of |value|. Overwrites any existing value stored in the
  // map for the key |policy|.
  void Set(ConfigurationPolicyType policy, Value* value);
  void Erase(ConfigurationPolicyType policy);

  void Swap(PolicyMap* other);

  bool Equals(const PolicyMap& other) const;
  bool empty() const;
  size_t size() const;

  const_iterator begin() const;
  const_iterator end() const;
  void Clear();

 private:
  // Helper function for Equals(...).
  static bool MapEntryEquals(const PolicyMapType::value_type& a,
                             const PolicyMapType::value_type& b);

  PolicyMapType map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyMap);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_MAP_H_
