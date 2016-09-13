// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/client_policy_controller.h"

#include <utility>

#include "base/time/time.h"
#include "components/offline_pages/client_namespace_constants.h"

using LifetimeType = offline_pages::LifetimePolicy::LifetimeType;

namespace offline_pages {

ClientPolicyController::ClientPolicyController() {
  policies_.clear();
  // Manually defining client policies for bookmark and last_n.
  policies_.insert(std::make_pair(
      kBookmarkNamespace,
      MakePolicy(kBookmarkNamespace, LifetimeType::TEMPORARY,
                 base::TimeDelta::FromDays(7), kUnlimitedPages, 1)));
  policies_.insert(std::make_pair(
      kLastNNamespace, MakePolicy(kLastNNamespace, LifetimeType::TEMPORARY,
                                  base::TimeDelta::FromDays(2), kUnlimitedPages,
                                  kUnlimitedPages)));
  policies_.insert(std::make_pair(
      kAsyncNamespace,
      OfflinePageClientPolicyBuilder(kAsyncNamespace, LifetimeType::PERSISTENT,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetIsSupportedByDownload(true)
          .SetIsRemovedOnCacheReset(false)
          .Build()));
  policies_.insert(std::make_pair(
      kCCTNamespace,
      MakePolicy(kCCTNamespace, LifetimeType::TEMPORARY,
                 base::TimeDelta::FromDays(2), kUnlimitedPages, 1)));
  policies_.insert(std::make_pair(
      kDownloadNamespace, OfflinePageClientPolicyBuilder(
                              kDownloadNamespace, LifetimeType::PERSISTENT,
                              kUnlimitedPages, kUnlimitedPages)
                              .SetIsRemovedOnCacheReset(false)
                              .SetIsSupportedByDownload(true)
                              .Build()));
  policies_.insert(std::make_pair(
      kNTPSuggestionsNamespace,
      OfflinePageClientPolicyBuilder(kNTPSuggestionsNamespace,
                                     LifetimeType::PERSISTENT, kUnlimitedPages,
                                     kUnlimitedPages)
          .Build()));

  // Fallback policy.
  policies_.insert(std::make_pair(
      kDefaultNamespace, MakePolicy(kDefaultNamespace, LifetimeType::TEMPORARY,
                                    base::TimeDelta::FromDays(1), 10, 1)));
}

ClientPolicyController::~ClientPolicyController() {}

// static
const OfflinePageClientPolicy ClientPolicyController::MakePolicy(
    const std::string& name_space,
    LifetimeType lifetime_type,
    const base::TimeDelta& expire_period,
    size_t page_limit,
    size_t pages_allowed_per_url) {
  return OfflinePageClientPolicyBuilder(name_space, lifetime_type, page_limit,
                                        pages_allowed_per_url)
      .SetExpirePeriod(expire_period)
      .Build();
}

const OfflinePageClientPolicy& ClientPolicyController::GetPolicy(
    const std::string& name_space) const {
  const auto& iter = policies_.find(name_space);
  if (iter != policies_.end())
    return iter->second;
  // Fallback when the namespace isn't defined.
  return policies_.at(kDefaultNamespace);
}

bool ClientPolicyController::IsRemovedOnCacheReset(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_removed_on_cache_reset;
}

bool ClientPolicyController::IsSupportedByDownload(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_supported_by_download;
}

}  // namespace offline_pages
