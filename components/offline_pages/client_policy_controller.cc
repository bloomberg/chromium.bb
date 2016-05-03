// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/client_policy_controller.h"

#include <utility>

#include "base/time/time.h"

using LifetimeType = offline_pages::LifetimePolicy::LifetimeType;

namespace offline_pages {

namespace {
const char kBookmarkNamespace[] = "bookmark";
const char kLastNNamespace[] = "last_n";
}  // namespace

ClientPolicyController::ClientPolicyController() {
  policies_.clear();
  // Manually defining client policies for bookmark and last_n.
  policies_.insert(std::make_pair(
      kBookmarkNamespace,
      MakePolicy(kBookmarkNamespace, LifetimeType::TEMPORARY,
                 base::TimeDelta::FromDays(7), kUnlimitedPages)));
  policies_.insert(std::make_pair(
      kLastNNamespace, MakePolicy(kLastNNamespace, LifetimeType::TEMPORARY,
                                  base::TimeDelta::FromDays(2), 20)));
  // Fallback policy.
  policies_.insert(std::make_pair(
      kDefaultNamespace, MakePolicy(kDefaultNamespace, LifetimeType::TEMPORARY,
                                    base::TimeDelta::FromDays(1), 10)));
}

ClientPolicyController::~ClientPolicyController() {}

// static
const OfflinePageClientPolicy ClientPolicyController::MakePolicy(
    const std::string& name_space,
    LifetimeType lifetime_type,
    const base::TimeDelta& expire_period,
    int page_limit) {
  OfflinePageClientPolicy policy(
      {name_space, {lifetime_type, expire_period, page_limit}});
  return policy;
}

const OfflinePageClientPolicy& ClientPolicyController::GetPolicy(
    const std::string& name_space) const {
  const auto& iter = policies_.find(name_space);
  if (iter != policies_.end())
    return iter->second;
  // Fallback when the namespace isn't defined.
  return policies_.at(kDefaultNamespace);
}

}  // namespace offline_pages
