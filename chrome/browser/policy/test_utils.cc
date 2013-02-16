// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/test_utils.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"

namespace policy {

bool PolicyServiceIsEmpty(const PolicyService* service) {
  const PolicyMap& map = service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  if (!map.empty()) {
    base::DictionaryValue dict;
    for (PolicyMap::const_iterator it = map.begin(); it != map.end(); ++it)
      dict.SetWithoutPathExpansion(it->first, it->second.value->DeepCopy());
    LOG(WARNING) << "There are pre-existing policies in this machine: " << dict;
  }
  return map.empty();
}

}  // namespace policy
