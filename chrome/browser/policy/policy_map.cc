// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_map.h"

#include <algorithm>

#include "base/stl_util-inl.h"

namespace policy {

PolicyMap::PolicyMap() {
}

PolicyMap::~PolicyMap() {
  Clear();
}

const Value* PolicyMap::Get(ConfigurationPolicyType policy) const {
  PolicyMapType::const_iterator entry = map_.find(policy);
  return entry == map_.end() ? NULL : entry->second;
}

void PolicyMap::Set(ConfigurationPolicyType policy, Value* value) {
  std::swap(map_[policy], value);
  delete value;
}

void PolicyMap::Erase(ConfigurationPolicyType policy) {
  const const_iterator entry = map_.find(policy);
  if (entry != map_.end()) {
    delete entry->second;
    map_.erase(entry->first);
  }
}

void PolicyMap::Swap(PolicyMap* other) {
  map_.swap(other->map_);
}

bool PolicyMap::Equals(const PolicyMap& other) const {
  return other.map_.size() == map_.size() &&
      std::equal(map_.begin(), map_.end(), other.map_.begin(), MapEntryEquals);
}

bool PolicyMap::empty() const {
  return map_.empty();
}

size_t PolicyMap::size() const {
  return map_.size();
}

PolicyMap::const_iterator PolicyMap::begin() const {
  return map_.begin();
}

PolicyMap::const_iterator PolicyMap::end() const {
  return map_.end();
}

void PolicyMap::Clear() {
  STLDeleteValues(&map_);
}

// static
bool PolicyMap::MapEntryEquals(const PolicyMap::PolicyMapType::value_type& a,
                               const PolicyMap::PolicyMapType::value_type& b) {
  return a.first == b.first && Value::Equals(a.second, b.second);
}

}  // namespace policy
