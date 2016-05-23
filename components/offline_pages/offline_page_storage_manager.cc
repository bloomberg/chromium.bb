// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_storage_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_client_policy.h"
#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

OfflinePageStorageManager::OfflinePageStorageManager(
    Client* client,
    ClientPolicyController* policy_controller,
    ArchiveManager* archive_manager)
    : client_(client),
      policy_controller_(policy_controller),
      archive_manager_(archive_manager),
      in_progress_(false),
      clock_(new base::DefaultClock()),
      weak_ptr_factory_(this) {}

OfflinePageStorageManager::~OfflinePageStorageManager() {}

void OfflinePageStorageManager::ClearPagesIfNeeded(
    const ClearPagesCallback& callback) {
  if (in_progress_)
    return;
  in_progress_ = true;
  archive_manager_->GetStorageStats(
      base::Bind(&OfflinePageStorageManager::OnGetStorageStatsDone,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageStorageManager::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void OfflinePageStorageManager::OnGetStorageStatsDone(
    const ClearPagesCallback& callback,
    const ArchiveManager::StorageStats& stats) {
  DCHECK(in_progress_);
  ClearMode mode = ShouldClearPages(stats);
  if (mode == ClearMode::NOT_NEEDED) {
    in_progress_ = false;
    last_clear_time_ = clock_->Now();
    callback.Run(0, ClearStorageResult::UNNECESSARY);
    return;
  }
  client_->GetAllPages(base::Bind(&OfflinePageStorageManager::OnGetAllPagesDone,
                                  weak_ptr_factory_.GetWeakPtr(), callback,
                                  stats));
}

void OfflinePageStorageManager::OnGetAllPagesDone(
    const ClearPagesCallback& callback,
    const ArchiveManager::StorageStats& stats,
    const MultipleOfflinePageItemResult& pages) {
  std::vector<int64_t> offline_ids;
  GetExpiredPageIds(pages, stats, offline_ids);
  client_->DeletePagesByOfflineId(
      offline_ids, base::Bind(&OfflinePageStorageManager::OnExpiredPagesDeleted,
                              weak_ptr_factory_.GetWeakPtr(), callback,
                              static_cast<int>(offline_ids.size())));
}

void OfflinePageStorageManager::OnExpiredPagesDeleted(
    const ClearPagesCallback& callback,
    int pages_cleared,
    DeletePageResult result) {
  last_clear_time_ = clock_->Now();
  ClearStorageResult clear_result = result == DeletePageResult::SUCCESS
                                        ? ClearStorageResult::SUCCESS
                                        : ClearStorageResult::DELETE_FAILURE;
  in_progress_ = false;
  callback.Run(pages_cleared, clear_result);
}

void OfflinePageStorageManager::GetExpiredPageIds(
    const MultipleOfflinePageItemResult& pages,
    const ArchiveManager::StorageStats& stats,
    std::vector<int64_t>& offline_ids) {
  base::Time now = clock_->Now();

  // Creating a map from namespace to a vector of page items.
  // Sort each vector based on last accessed time and all pages after index
  // min{size(), page_limit} should be expired. And then start iterating
  // backwards to expire pages.
  std::map<std::string, std::vector<OfflinePageItem>> pages_map;
  std::vector<OfflinePageItem> kept_pages;
  int64_t kept_pages_size = 0;

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

    int page_list_size = static_cast<int>(page_list.size());
    int pos = 0;
    while (pos < page_list_size &&
           (policy.page_limit == kUnlimitedPages || pos < policy.page_limit) &&
           !ShouldBeExpired(now, page_list.at(pos))) {
      kept_pages_size += page_list.at(pos).file_size;
      kept_pages.push_back(page_list.at(pos));
      pos++;
    }

    for (; pos < page_list_size; pos++)
      offline_ids.push_back(page_list.at(pos).offline_id);
  }

  // If we're still over the clear threshold, we're going to clear remaining
  // pages from oldest last access time.
  int64_t free_space = stats.free_disk_space;
  int64_t total_size = stats.total_archives_size;
  int64_t space_to_release =
      kept_pages_size -
      (total_size + free_space) * kOfflinePageStorageClearThreshold;
  if (space_to_release > 0) {
    // Here we're sorting the |kept_pages| with oldest first.
    std::sort(kept_pages.begin(), kept_pages.end(),
              [](const OfflinePageItem& a, const OfflinePageItem& b) -> bool {
                return a.last_access_time < b.last_access_time;
              });
    int kept_pages_size = static_cast<int>(kept_pages.size());
    int pos = 0;
    while (pos < kept_pages_size && space_to_release > 0) {
      space_to_release -= kept_pages.at(pos).file_size;
      offline_ids.push_back(kept_pages.at(pos).offline_id);
      pos++;
    }
  }
}

OfflinePageStorageManager::ClearMode
OfflinePageStorageManager::ShouldClearPages(
    const ArchiveManager::StorageStats& storage_stats) {
  int64_t total_size = storage_stats.total_archives_size;
  int64_t free_space = storage_stats.free_disk_space;
  if (total_size == 0)
    return ClearMode::NOT_NEEDED;

  // If the size of all offline pages is more than limit, or it's larger than a
  // specified percentage of all available storage space on the disk we'll clear
  // all offline pages.
  if (total_size >= (total_size + free_space) * kOfflinePageStorageLimit)
    return ClearMode::DEFAULT;
  // If it's been more than the pre-defined interval since the last time we
  // clear the storage, we should clear pages.
  if (last_clear_time_ == base::Time() ||
      clock_->Now() - last_clear_time_ >= kClearStorageInterval) {
    return ClearMode::DEFAULT;
  }

  // Otherwise there's no need to clear storage right now.
  return ClearMode::NOT_NEEDED;
}

bool OfflinePageStorageManager::ShouldBeExpired(const base::Time& now,
                                                const OfflinePageItem& page) {
  const LifetimePolicy& policy =
      policy_controller_->GetPolicy(page.client_id.name_space).lifetime_policy;
  return now - page.last_access_time >= policy.expiration_period;
}

}  // namespace offline_pages
