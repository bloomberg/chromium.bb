// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_BUNDLE_H_
#define CHROME_BROWSER_POLICY_POLICY_BUNDLE_H_
#pragma once

#include <map>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "chrome/browser/policy/policy_service.h"

namespace policy {

class PolicyMap;

// Maps policy namespaces to PolicyMaps.
class PolicyBundle {
 public:
  // Groups a policy domain and a component ID in a single object representing
  // a policy namespace. Used as the key type in MapType.
  typedef std::pair<PolicyDomain, std::string> PolicyNamespace;

  typedef std::map<PolicyNamespace, PolicyMap*> MapType;
  typedef MapType::const_iterator const_iterator;

  PolicyBundle();
  virtual ~PolicyBundle();

  // Returns the PolicyMap for namespace (domain, component_id).
  PolicyMap& Get(PolicyDomain domain, const std::string& component_id);
  const PolicyMap& Get(PolicyDomain domain,
                       const std::string& component_id) const;

  // Swaps the internal representation of |this| with |other|.
  void Swap(PolicyBundle* other);

  // |this| becomes a copy of |other|. Any existing PolicyMaps are dropped.
  void CopyFrom(const PolicyBundle& other);

  // Merges the PolicyMaps of |this| with those of |other| for each namespace
  // in common. Also adds copies of the (namespace, PolicyMap) pairs in |other|
  // that don't have an entry in |this|.
  // Each policy in each PolicyMap is replaced only if the policy from |other|
  // has a higher priority.
  // See PolicyMap::MergeFrom for details on merging individual PolicyMaps.
  void MergeFrom(const PolicyBundle& other);

  // Returns true if |other| has the same keys and value as |this|.
  bool Equals(const PolicyBundle& other) const;

  // Returns iterators to the beginning and end of the underlying container.
  // These can be used to iterate over and read the PolicyMaps, but not to
  // modify them.
  const_iterator begin() const;
  const_iterator end() const;

  // Erases all the existing pairs.
  void Clear();

 private:
  MapType policy_bundle_;

  // An empty PolicyMap that is returned by const Get() for namespaces that
  // do not exist in |policy_bundle_|.
  const PolicyMap kEmpty_;

  DISALLOW_COPY_AND_ASSIGN(PolicyBundle);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_BUNDLE_H_
