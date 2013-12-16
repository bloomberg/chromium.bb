// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_MAP_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_MAP_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace policy {

// A mapping of policy names to policy values for a given policy namespace.
class POLICY_EXPORT PolicyMap {
 public:
  // Each policy maps to an Entry which keeps the policy value as well as other
  // relevant data about the policy.
  struct POLICY_EXPORT Entry {
    PolicyLevel level;
    PolicyScope scope;
    base::Value* value;
    ExternalDataFetcher* external_data_fetcher;

    Entry();

    // Deletes all members owned by |this|.
    void DeleteOwnedMembers();

    // Returns a copy of |this|.
    scoped_ptr<Entry> DeepCopy() const;

    // Returns true if |this| has higher priority than |other|.
    bool has_higher_priority_than(const Entry& other) const;

    // Returns true if |this| equals |other|.
    bool Equals(const Entry& other) const;
  };

  typedef std::map<std::string, Entry> PolicyMapType;
  typedef PolicyMapType::const_iterator const_iterator;

  PolicyMap();
  virtual ~PolicyMap();

  // Returns a weak reference to the entry currently stored for key |policy|,
  // or NULL if not found. Ownership is retained by the PolicyMap.
  const Entry* Get(const std::string& policy) const;

  // Returns a weak reference to the value currently stored for key |policy|,
  // or NULL if not found. Ownership is retained by the PolicyMap.
  // This is equivalent to Get(policy)->value, when it doesn't return NULL.
  const base::Value* GetValue(const std::string& policy) const;

  // Takes ownership of |value| and |external_data_fetcher|. Overwrites any
  // existing information stored in the map for the key |policy|.
  void Set(const std::string& policy,
           PolicyLevel level,
           PolicyScope scope,
           base::Value* value,
           ExternalDataFetcher* external_data_fetcher);

  // Erase the given |policy|, if it exists in this map.
  void Erase(const std::string& policy);

  // Swaps the internal representation of |this| with |other|.
  void Swap(PolicyMap* other);

  // |this| becomes a copy of |other|. Any existing policies are dropped.
  void CopyFrom(const PolicyMap& other);

  // Returns a copy of |this|.
  scoped_ptr<PolicyMap> DeepCopy() const;

  // Merges policies from |other| into |this|. Existing policies are only
  // overridden by those in |other| if they have a higher priority, as defined
  // by Entry::has_higher_priority_than(). If a policy is contained in both
  // maps with the same priority, the current value in |this| is preserved.
  void MergeFrom(const PolicyMap& other);

  // Loads the values in |policies| into this PolicyMap. All policies loaded
  // will have |level| and |scope| in their entries. Existing entries are
  // replaced.
  void LoadFrom(const base::DictionaryValue* policies,
                PolicyLevel level,
                PolicyScope scope);

  // Compares this value map against |other| and stores all key names that have
  // different values or reference different external data in |differing_keys|.
  // This includes keys that are present only in one of the maps.
  // |differing_keys| is not cleared before the keys are added.
  void GetDifferingKeys(const PolicyMap& other,
                        std::set<std::string>* differing_keys) const;

  // Removes all policies that don't have the specified |level|. This is a
  // temporary helper method, until mandatory and recommended levels are served
  // by a single provider.
  // TODO(joaodasilva): Remove this. http://crbug.com/108999
  void FilterLevel(PolicyLevel level);

  bool Equals(const PolicyMap& other) const;
  bool empty() const;
  size_t size() const;

  const_iterator begin() const;
  const_iterator end() const;
  void Clear();

 private:
  // Helper function for Equals().
  static bool MapEntryEquals(const PolicyMapType::value_type& a,
                             const PolicyMapType::value_type& b);

  PolicyMapType map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyMap);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_MAP_H_
