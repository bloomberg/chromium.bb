// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_storage_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_client_policy.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_types.h"

namespace offline_pages {

OfflinePageStorageManager::OfflinePageStorageManager(
    Client* client,
    ClientPolicyController* policy_controller)
    : client_(client),
      policy_controller_(policy_controller),
      in_progress_(false),
      clock_(new base::DefaultClock()),
      weak_ptr_factory_(this) {}

OfflinePageStorageManager::~OfflinePageStorageManager() {}

void OfflinePageStorageManager::ClearPagesIfNeeded(
    const ClearPageCallback& callback) {
  if (!ShouldClearPages())
    return;
  in_progress_ = true;
  client_->GetAllPages(base::Bind(&OfflinePageStorageManager::ClearExpiredPages,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageStorageManager::ClearExpiredPages(
    const ClearPageCallback& callback,
    const MultipleOfflinePageItemResult& pages) {
  DCHECK(in_progress_);
  std::vector<int64_t> offline_ids;
  GetExpiredPageIds(pages, offline_ids);
  client_->DeletePagesByOfflineId(
      offline_ids,
      base::Bind(&OfflinePageStorageManager::OnExpiredPagesDeleted,
                 weak_ptr_factory_.GetWeakPtr(), callback, offline_ids.size()));
}

void OfflinePageStorageManager::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void OfflinePageStorageManager::GetExpiredPageIds(
    const MultipleOfflinePageItemResult& pages,
    std::vector<int64_t>& offline_ids) {
  base::Time now = clock_->Now();

  // Creating a map from namespace to a vector of page items.
  // Sort each vector based on last accessed time and all pages after index
  // min{size(), page_limit} should be expired. And then start iterating
  // backwards to expire pages.
  std::map<std::string, std::vector<OfflinePageItem>> pages_map;

  for (const auto& page : pages)
    pages_map[page.client_id.name_space].push_back(page);

  for (auto& iter : pages_map) {
    std::string name_space = iter.first;
    std::vector<OfflinePageItem>& page_list = iter.second;

    LifetimePolicy policy =
        policy_controller_->GetPolicy(name_space).lifetime_policy;

    std::sort(page_list.begin(), page_list.end(),
              [](const OfflinePageItem& a, const OfflinePageItem& b) -> bool {
                return a.last_access_time > b.last_access_time;
              });

    int page_list_size = page_list.size();
    int pos = 0;
    while (pos < page_list_size &&
           (policy.page_limit == kUnlimitedPages || pos < policy.page_limit) &&
           !ShouldBeExpired(now, page_list.at(pos))) {
      pos++;
    }

    for (int i = pos; i < page_list_size; i++)
      offline_ids.push_back(page_list.at(i).offline_id);
  }
}

void OfflinePageStorageManager::OnExpiredPagesDeleted(
    const ClearPageCallback& callback,
    int pages_cleared,
    DeletePageResult result) {
  in_progress_ = false;
  callback.Run(pages_cleared, result);
}

bool OfflinePageStorageManager::ShouldClearPages() {
  return !in_progress_;
}

bool OfflinePageStorageManager::ShouldBeExpired(const base::Time& now,
                                                const OfflinePageItem& page) {
  const LifetimePolicy& policy =
      policy_controller_->GetPolicy(page.client_id.name_space).lifetime_policy;
  return now - page.last_access_time > policy.expiration_period;
}

}  // namespace offline_pages
