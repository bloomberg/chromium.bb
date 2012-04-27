// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_service.h"

#include "base/values.h"

namespace policy {

PolicyChangeRegistrar::PolicyChangeRegistrar(PolicyService* policy_service,
                                             PolicyDomain domain,
                                             const std::string& component_id)
    : policy_service_(policy_service),
      domain_(domain),
      component_id_(component_id) {}

PolicyChangeRegistrar::~PolicyChangeRegistrar() {
  if (!callback_map_.empty())
    policy_service_->RemoveObserver(domain_, component_id_, this);
}

void PolicyChangeRegistrar::Observe(const std::string& policy_name,
                                    const UpdateCallback& callback) {
  if (callback_map_.empty())
    policy_service_->AddObserver(domain_, component_id_, this);
  callback_map_[policy_name] = callback;
}

void PolicyChangeRegistrar::OnPolicyUpdated(PolicyDomain domain,
                                            const std::string& component_id,
                                            const PolicyMap& previous,
                                            const PolicyMap& current) {
  for (CallbackMap::iterator it = callback_map_.begin();
       it != callback_map_.end(); ++it) {
    const Value* prev = previous.GetValue(it->first);
    const Value* cur = current.GetValue(it->first);
    if (!base::Value::Equals(prev, cur))
      it->second.Run(prev, cur);
  }
}

}  // namespace policy
