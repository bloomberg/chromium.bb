// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_export.h"

namespace policy {

class POLICY_EXPORT PolicyMerger {
 public:
  PolicyMerger();
  virtual ~PolicyMerger();

  virtual void Merge(PolicyMap::PolicyMapType* result) const;
  virtual bool CanMerge(const std::string& policy_name,
                        PolicyMap::Entry& policy) const = 0;

 protected:
  virtual PolicyMap::Entry Merge(const PolicyMap::Entry& policy) const = 0;
};

class POLICY_EXPORT PolicyListMerger : public PolicyMerger {
 public:
  explicit PolicyListMerger(const std::set<std::string> policies_to_merge);

  ~PolicyListMerger() override;
  bool CanMerge(const std::string& policy_name,
                PolicyMap::Entry& policy) const override;

 protected:
  PolicyMap::Entry Merge(const PolicyMap::Entry& policy) const override;

 private:
  const std::set<std::string> policies_to_merge_;
  DISALLOW_COPY_AND_ASSIGN(PolicyListMerger);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_MERGER_H_
