// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_service.h"

#include "base/values.h"

namespace policy {

PolicyNamespace::PolicyNamespace() {}

PolicyNamespace::PolicyNamespace(PolicyDomain domain,
                                 const std::string& component_id)
    : domain(domain),
      component_id(component_id) {}

PolicyNamespace::PolicyNamespace(const PolicyNamespace& other)
    : domain(other.domain),
      component_id(other.component_id) {}

PolicyNamespace::~PolicyNamespace() {}

PolicyNamespace& PolicyNamespace::operator=(const PolicyNamespace& other) {
  domain = other.domain;
  component_id = other.component_id;
  return *this;
}

bool PolicyNamespace::operator<(const PolicyNamespace& other) const {
  return domain < other.domain ||
         (domain == other.domain && component_id < other.component_id);
}

bool PolicyNamespace::operator==(const PolicyNamespace& other) const {
  return domain == other.domain && component_id == other.component_id;
}

bool PolicyNamespace::operator!=(const PolicyNamespace& other) const {
  return !(*this == other);
}

PolicyChangeRegistrar::PolicyChangeRegistrar(PolicyService* policy_service,
                                             const PolicyNamespace& ns)
    : policy_service_(policy_service),
      ns_(ns) {}

PolicyChangeRegistrar::~PolicyChangeRegistrar() {
  if (!callback_map_.empty())
    policy_service_->RemoveObserver(ns_.domain, this);
}

void PolicyChangeRegistrar::Observe(const std::string& policy_name,
                                    const UpdateCallback& callback) {
  if (callback_map_.empty())
    policy_service_->AddObserver(ns_.domain, this);
  callback_map_[policy_name] = callback;
}

void PolicyChangeRegistrar::OnPolicyUpdated(const PolicyNamespace& ns,
                                            const PolicyMap& previous,
                                            const PolicyMap& current) {
  if (ns != ns_)
    return;
  for (CallbackMap::iterator it = callback_map_.begin();
       it != callback_map_.end(); ++it) {
    const Value* prev = previous.GetValue(it->first);
    const Value* cur = current.GetValue(it->first);
    if (!base::Value::Equals(prev, cur))
      it->second.Run(prev, cur);
  }
}

}  // namespace policy
