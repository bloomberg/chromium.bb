// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_DOMAIN_DESCRIPTOR_H_
#define CHROME_BROWSER_POLICY_POLICY_DOMAIN_DESCRIPTOR_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"

namespace policy {

class PolicyBundle;

// For each policy domain, this class keeps the complete list of valid
// components for that domain, and the Schema for each component.
class PolicyDomainDescriptor
    : public base::RefCountedThreadSafe<PolicyDomainDescriptor> {
 public:
  typedef std::map<std::string, Schema> SchemaMap;

  explicit PolicyDomainDescriptor(PolicyDomain domain);

  PolicyDomain domain() const { return domain_; }
  const SchemaMap& components() const { return schema_map_; }

  // Registers the given |component_id| for this domain, and sets its current
  // |schema|. This registration overrides any previously set schema for this
  // component.
  void RegisterComponent(const std::string& component_id, Schema schema);

  // Removes all the policies in |bundle| that don't match this descriptor.
  // Policies of domains other than |domain_| are ignored.
  void FilterBundle(PolicyBundle* bundle) const;

 private:
  friend class base::RefCountedThreadSafe<PolicyDomainDescriptor>;

  ~PolicyDomainDescriptor();

  PolicyDomain domain_;
  SchemaMap schema_map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyDomainDescriptor);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_DOMAIN_DESCRIPTOR_H_
