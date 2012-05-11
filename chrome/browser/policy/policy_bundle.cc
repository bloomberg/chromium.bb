// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_bundle.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

PolicyBundle::PolicyBundle() {}

PolicyBundle::~PolicyBundle() {
  Clear();
}

PolicyMap& PolicyBundle::Get(PolicyDomain domain,
                             const std::string& component_id) {
  DCHECK(domain != POLICY_DOMAIN_CHROME || component_id.empty());
  PolicyMap*& policy = policy_bundle_[PolicyNamespace(domain, component_id)];
  if (!policy)
    policy = new PolicyMap();
  return *policy;
}

const PolicyMap& PolicyBundle::Get(PolicyDomain domain,
                                   const std::string& component_id) const {
  DCHECK(domain != POLICY_DOMAIN_CHROME || component_id.empty());
  PolicyNamespace ns(domain, component_id);
  const_iterator it = policy_bundle_.find(ns);
  return it == end() ? kEmpty_ : *it->second;
}

void PolicyBundle::Swap(PolicyBundle* other) {
  policy_bundle_.swap(other->policy_bundle_);
}

void PolicyBundle::CopyFrom(const PolicyBundle& other) {
  Clear();
  for (PolicyBundle::const_iterator it = other.begin();
       it != other.end(); ++it) {
    PolicyMap*& policy = policy_bundle_[it->first];
    policy = new PolicyMap();
    policy->CopyFrom(*it->second);
  }
}

void PolicyBundle::MergeFrom(const PolicyBundle& other) {
  // Iterate over both |this| and |other| in order; skip what's extra in |this|,
  // add what's missing, and merge the namespaces in common.
  MapType::iterator it_this = policy_bundle_.begin();
  MapType::iterator end_this = policy_bundle_.end();
  const_iterator it_other = other.begin();
  const_iterator end_other = other.end();

  while (it_this != end_this && it_other != end_other) {
    if (it_this->first == it_other->first) {
      // Same namespace: merge existing PolicyMaps.
      it_this->second->MergeFrom(*it_other->second);
      ++it_this;
      ++it_other;
    } else if (it_this->first < it_other->first) {
      // |this| has a PolicyMap that |other| doesn't; skip it.
      ++it_this;
    } else if (it_this->first > it_other->first) {
      // |other| has a PolicyMap that |this| doesn't; copy it.
      PolicyMap*& policy = policy_bundle_[it_other->first];
      DCHECK(!policy);
      policy = new PolicyMap();
      policy->CopyFrom(*it_other->second);
      ++it_other;
    } else {
      NOTREACHED();
    }
  }

  // Add extra PolicyMaps at the end.
  while (it_other != end_other) {
    PolicyMap*& policy = policy_bundle_[it_other->first];
    DCHECK(!policy);
    policy = new PolicyMap();
    policy->CopyFrom(*it_other->second);
    ++it_other;
  }
}

PolicyBundle::const_iterator PolicyBundle::begin() const {
  return policy_bundle_.begin();
}

PolicyBundle::const_iterator PolicyBundle::end() const {
  return policy_bundle_.end();
}

void PolicyBundle::Clear() {
  STLDeleteValues(&policy_bundle_);
}

}  // namespace policy
