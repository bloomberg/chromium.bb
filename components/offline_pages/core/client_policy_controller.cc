// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/client_policy_controller.h"

#include <utility>

#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"

namespace offline_pages {
namespace {

// Generates a client policy from the input values.
const OfflinePageClientPolicy MakePolicy(const std::string& name_space,
                                         LifetimeType lifetime_type,
                                         const base::TimeDelta& expire_period,
                                         size_t page_limit,
                                         size_t pages_allowed_per_url) {
  return OfflinePageClientPolicyBuilder(name_space, lifetime_type, page_limit,
                                        pages_allowed_per_url)
      .SetExpirePeriod(expire_period)
      .Build();
}

}  // namespace

ClientPolicyController::ClientPolicyController() {
  // Manually defining client policies for bookmark and last_n.
  policies_.emplace(
      kBookmarkNamespace,
      MakePolicy(kBookmarkNamespace, LifetimeType::TEMPORARY,
                 base::TimeDelta::FromDays(7), kUnlimitedPages, 1));
  policies_.emplace(
      kLastNNamespace,
      OfflinePageClientPolicyBuilder(kLastNNamespace, LifetimeType::TEMPORARY,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetExpirePeriod(base::TimeDelta::FromDays(30))
          .SetIsRestrictedToTabFromClientId(true)
          .Build());
  policies_.emplace(
      kAsyncNamespace,
      OfflinePageClientPolicyBuilder(kAsyncNamespace, LifetimeType::PERSISTENT,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetIsSupportedByDownload(true)
          .Build());
  policies_.emplace(
      kCCTNamespace,
      OfflinePageClientPolicyBuilder(kCCTNamespace, LifetimeType::TEMPORARY,
                                     kUnlimitedPages, 1)
          .SetExpirePeriod(base::TimeDelta::FromDays(2))
          .SetRequiresSpecificUserSettings(true)
          .Build());
  policies_.emplace(kDownloadNamespace,
                    OfflinePageClientPolicyBuilder(
                        kDownloadNamespace, LifetimeType::PERSISTENT,
                        kUnlimitedPages, kUnlimitedPages)
                        .SetIsSupportedByDownload(true)
                        .Build());
  policies_.emplace(kNTPSuggestionsNamespace,
                    OfflinePageClientPolicyBuilder(
                        kNTPSuggestionsNamespace, LifetimeType::PERSISTENT,
                        kUnlimitedPages, kUnlimitedPages)
                        .SetIsSupportedByDownload(true)
                        .Build());
  policies_.emplace(
      kSuggestedArticlesNamespace,
      OfflinePageClientPolicyBuilder(kSuggestedArticlesNamespace,
                                     LifetimeType::TEMPORARY, kUnlimitedPages,
                                     kUnlimitedPages)
          .SetExpirePeriod(base::TimeDelta::FromDays(30))
          .SetIsSupportedByDownload(IsPrefetchingOfflinePagesEnabled())
          .SetIsSuggested(true)
          .Build());
  policies_.emplace(kBrowserActionsNamespace,
                    OfflinePageClientPolicyBuilder(
                        kBrowserActionsNamespace, LifetimeType::PERSISTENT,
                        kUnlimitedPages, kUnlimitedPages)
                        .SetIsSupportedByDownload(true)
                        .SetAllowConversionToBackgroundFileDownload(true)
                        .Build());
  policies_.emplace(kLivePageSharingNamespace,
                    OfflinePageClientPolicyBuilder(kLivePageSharingNamespace,
                                                   LifetimeType::TEMPORARY,
                                                   kUnlimitedPages, 1)
                        .SetExpirePeriod(base::TimeDelta::FromHours(1))
                        .SetIsRestrictedToTabFromClientId(true)
                        .Build());
  policies_.emplace(
      kAutoAsyncNamespace,
      OfflinePageClientPolicyBuilder(
          kAutoAsyncNamespace, LifetimeType::TEMPORARY, kUnlimitedPages, 1)
          .SetExpirePeriod(base::TimeDelta::FromDays(30))
          .SetDeferBackgroundFetchWhilePageIsActive(true)
          .Build());

  // Fallback policy.
  policies_.emplace(kDefaultNamespace,
                    MakePolicy(kDefaultNamespace, LifetimeType::TEMPORARY,
                               base::TimeDelta::FromDays(1), 10, 1));

  for (const auto& policy_item : policies_) {
    const std::string& name = policy_item.first;
    switch (policy_item.second.lifetime_policy.lifetime_type) {
      case LifetimeType::TEMPORARY:
        temporary_namespaces_.push_back(name);
        break;
      case LifetimeType::PERSISTENT:
        persistent_namespaces_.push_back(name);
        break;
    }
  }
}

ClientPolicyController::~ClientPolicyController() {}

const OfflinePageClientPolicy& ClientPolicyController::GetPolicy(
    const std::string& name_space) const {
  const auto& iter = policies_.find(name_space);
  if (iter != policies_.end())
    return iter->second;
  // Fallback when the namespace isn't defined.
  return policies_.at(kDefaultNamespace);
}

std::vector<std::string> ClientPolicyController::GetAllNamespaces() const {
  std::vector<std::string> result;
  for (const auto& policy_item : policies_)
    result.emplace_back(policy_item.first);

  return result;
}

bool ClientPolicyController::IsTemporary(const std::string& name_space) const {
  return GetPolicy(name_space).lifetime_policy.lifetime_type ==
         LifetimeType::TEMPORARY;
}

const std::vector<std::string>& ClientPolicyController::GetTemporaryNamespaces()
    const {
  return temporary_namespaces_;
}

bool ClientPolicyController::IsSupportedByDownload(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_supported_by_download;
}

bool ClientPolicyController::IsPersistent(const std::string& name_space) const {
  return GetPolicy(name_space).lifetime_policy.lifetime_type ==
         LifetimeType::PERSISTENT;
}

const std::vector<std::string>&
ClientPolicyController::GetPersistentNamespaces() const {
  return persistent_namespaces_;
}

bool ClientPolicyController::IsRestrictedToTabFromClientId(
    const std::string& name_space) const {
  return GetPolicy(name_space)
      .feature_policy.is_restricted_to_tab_from_client_id;
}

bool ClientPolicyController::RequiresSpecificUserSettings(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.requires_specific_user_settings;
}

bool ClientPolicyController::IsSuggested(const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_suggested;
}

bool ClientPolicyController::AllowsConversionToBackgroundFileDownload(
    const std::string& name_space) const {
  return GetPolicy(name_space)
      .feature_policy.allows_conversion_to_background_file_download;
}

}  // namespace offline_pages
