// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_map.h"

#include <algorithm>

#include "base/stl_util.h"

namespace policy {

bool PolicyMap::Entry::has_higher_priority_than(
    const PolicyMap::Entry& other) const {
  if (level == other.level)
    return scope > other.scope;
  else
    return level > other.level;
}

bool PolicyMap::Entry::Equals(const PolicyMap::Entry& other) const {
  return level == other.level &&
         scope == other.scope &&
         Value::Equals(value, other.value);
}

PolicyMap::PolicyMap() {
}

PolicyMap::~PolicyMap() {
  Clear();
}

const PolicyMap::Entry* PolicyMap::Get(const std::string& policy) const {
  PolicyMapType::const_iterator entry = map_.find(policy);
  return entry == map_.end() ? NULL : &entry->second;
}

const Value* PolicyMap::GetValue(const std::string& policy) const {
  PolicyMapType::const_iterator entry = map_.find(policy);
  return entry == map_.end() ? NULL : entry->second.value;
}

void PolicyMap::Set(const std::string& policy,
                    PolicyLevel level,
                    PolicyScope scope,
                    Value* value) {
  Entry& entry = map_[policy];
  delete entry.value;
  entry.level = level;
  entry.scope = scope;
  entry.value = value;
}

void PolicyMap::Erase(const std::string& policy) {
  PolicyMapType::iterator it = map_.find(policy);
  if (it != map_.end()) {
    delete it->second.value;
    map_.erase(it);
  }
}

void PolicyMap::Swap(PolicyMap* other) {
  map_.swap(other->map_);
}

void PolicyMap::CopyFrom(const PolicyMap& other) {
  Clear();
  for (const_iterator it = other.begin(); it != other.end(); ++it) {
    const Entry& entry = it->second;
    Set(it->first, entry.level, entry.scope, entry.value->DeepCopy());
  }
}

scoped_ptr<PolicyMap> PolicyMap::DeepCopy() const {
  PolicyMap* copy = new PolicyMap();
  copy->CopyFrom(*this);
  return make_scoped_ptr(copy);
}

void PolicyMap::MergeFrom(const PolicyMap& other) {
  for (const_iterator it = other.begin(); it != other.end(); ++it) {
    const Entry* entry = Get(it->first);
    if (!entry || it->second.has_higher_priority_than(*entry)) {
      Set(it->first, it->second.level, it->second.scope,
          it->second.value->DeepCopy());
    }
  }
}

void PolicyMap::LoadFrom(
    const DictionaryValue* policies,
    PolicyLevel level,
    PolicyScope scope) {
  for (DictionaryValue::Iterator it(*policies); !it.IsAtEnd(); it.Advance())
    Set(it.key(), level, scope, it.value().DeepCopy());
}

void PolicyMap::GetDifferingKeys(const PolicyMap& other,
                                 std::set<std::string>* differing_keys) const {
  // Walk over the maps in lockstep, adding everything that is different.
  const_iterator iter_this(begin());
  const_iterator iter_other(other.begin());
  while (iter_this != end() && iter_other != other.end()) {
    const int diff = iter_this->first.compare(iter_other->first);
    if (diff == 0) {
      if (!iter_this->second.Equals(iter_other->second))
        differing_keys->insert(iter_this->first);
      ++iter_this;
      ++iter_other;
    } else if (diff < 0) {
      differing_keys->insert(iter_this->first);
      ++iter_this;
    } else {
      differing_keys->insert(iter_other->first);
      ++iter_other;
    }
  }

  // Add the remaining entries.
  for ( ; iter_this != end(); ++iter_this)
      differing_keys->insert(iter_this->first);
  for ( ; iter_other != other.end(); ++iter_other)
      differing_keys->insert(iter_other->first);
}

void PolicyMap::FilterLevel(PolicyLevel level) {
  PolicyMapType::iterator iter(map_.begin());
  while (iter != map_.end()) {
    if (iter->second.level != level) {
      delete iter->second.value;
      map_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

bool PolicyMap::Equals(const PolicyMap& other) const {
  return other.size() == size() &&
      std::equal(begin(), end(), other.begin(), MapEntryEquals);
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
  for (PolicyMapType::iterator it = map_.begin(); it != map_.end(); ++it)
    delete it->second.value;
  map_.clear();
}

// static
bool PolicyMap::MapEntryEquals(const PolicyMap::PolicyMapType::value_type& a,
                               const PolicyMap::PolicyMapType::value_type& b) {
  return a.first == b.first && a.second.Equals(b.second);
}

}  // namespace policy
