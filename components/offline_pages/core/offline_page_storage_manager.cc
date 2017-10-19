// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_storage_manager.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"

using LifetimeType = offline_pages::LifetimePolicy::LifetimeType;

namespace offline_pages {

constexpr double constants::kOfflinePageStorageLimit;
constexpr double constants::kOfflinePageStorageClearThreshold;
constexpr base::TimeDelta constants::kClearStorageInterval;
constexpr base::TimeDelta constants::kRemovePageItemInterval;

OfflinePageStorageManager::OfflinePageStorageManager(
    OfflinePageModel* model,
    ClientPolicyController* policy_controller,
    ArchiveManager* archive_manager)
    : model_(model),
      policy_controller_(policy_controller),
      archive_manager_(archive_manager),
      clock_(new base::DefaultClock()),
      reported_usage_this_launch_(false),
      weak_ptr_factory_(this) {}

OfflinePageStorageManager::~OfflinePageStorageManager() {}

void OfflinePageStorageManager::ClearPagesIfNeeded(
    const ClearStorageCallback& callback) {
  if (IsInProgress())
    return;
  clear_time_ = clock_->Now();
  archive_manager_->GetStorageStats(base::Bind(
      &OfflinePageStorageManager::OnGetStorageStatsDoneForClearingPages,
      weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageStorageManager::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void OfflinePageStorageManager::ResetUsageReportingFlagForTesting() {
  reported_usage_this_launch_ = false;
}

void OfflinePageStorageManager::OnGetStorageStatsDoneForClearingPages(
    const ClearStorageCallback& callback,
    const ArchiveManager::StorageStats& stats) {
  DCHECK(IsInProgress());
  ClearMode mode = ShouldClearPages(stats);
  if (mode == ClearMode::NOT_NEEDED) {
    last_clear_time_ = clear_time_;
    callback.Run(0, ClearStorageResult::UNNECESSARY);
    return;
  }
  model_->GetAllPages(
      base::Bind(&OfflinePageStorageManager::OnGetAllPagesDoneForClearingPages,
                 weak_ptr_factory_.GetWeakPtr(), callback, stats));
}

void OfflinePageStorageManager::OnGetAllPagesDoneForClearingPages(
    const ClearStorageCallback& callback,
    const ArchiveManager::StorageStats& stats,
    const MultipleOfflinePageItemResult& pages) {
  ReportStorageUsageUMA(pages);
  std::vector<int64_t> page_ids_to_clear;
  GetPageIdsToClear(pages, stats, &page_ids_to_clear);
  model_->DeletePagesByOfflineId(
      page_ids_to_clear,
      base::Bind(&OfflinePageStorageManager::OnExpiredPagesCleared,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 page_ids_to_clear.size()));
}

void OfflinePageStorageManager::OnExpiredPagesCleared(
    const ClearStorageCallback& callback,
    size_t pages_cleared,
    DeletePageResult result) {
  last_clear_time_ = clear_time_;
  ClearStorageResult clear_result = ClearStorageResult::SUCCESS;
  if (result != DeletePageResult::SUCCESS)
    clear_result = ClearStorageResult::DELETE_FAILURE;
  callback.Run(pages_cleared, clear_result);
}

void OfflinePageStorageManager::GetPageIdsToClear(
    const MultipleOfflinePageItemResult& pages,
    const ArchiveManager::StorageStats& stats,
    std::vector<int64_t>* page_ids_to_clear) {
  // Creating a map from namespace to a vector of page items.
  // Sort each vector based on last accessed time and all pages after index
  // min{size(), page_limit} should be deleted.
  std::map<std::string, std::vector<OfflinePageItem>> pages_map;
  std::vector<OfflinePageItem> kept_pages;
  int64_t kept_pages_size = 0;

  for (const auto& page : pages) {
    if (IsExpired(page))
      page_ids_to_clear->push_back(page.offline_id);
    else
      pages_map[page.client_id.name_space].push_back(page);
  }

  for (auto& iter : pages_map) {
    std::string name_space = iter.first;
    std::vector<OfflinePageItem>& page_list = iter.second;

    LifetimePolicy policy =
        policy_controller_->GetPolicy(name_space).lifetime_policy;
    // Storage manager only manages temporary offlined pages, so we shouldn't
    // clear any persistent pages here.
    if (policy.lifetime_type == LifetimeType::PERSISTENT)
      continue;

    std::sort(page_list.begin(), page_list.end(),
              [](const OfflinePageItem& a, const OfflinePageItem& b) -> bool {
                return a.last_access_time > b.last_access_time;
              });

    size_t page_list_size = page_list.size();
    size_t pos = 0;
    while (pos < page_list_size &&
           (policy.page_limit == kUnlimitedPages || pos < policy.page_limit)) {
      kept_pages_size += page_list.at(pos).file_size;
      kept_pages.push_back(page_list.at(pos));
      pos++;
    }

    for (; pos < page_list_size; pos++)
      page_ids_to_clear->push_back(page_list.at(pos).offline_id);
  }

  // If we're still over the clear threshold, we're going to clear remaining
  // pages from oldest last access time.
  int64_t free_space = stats.free_disk_space;
  int64_t total_size = stats.temporary_archives_size;
  int64_t space_to_release =
      kept_pages_size -
      (total_size + free_space) * constants::kOfflinePageStorageClearThreshold;
  if (space_to_release > 0) {
    // Here we're sorting the |kept_pages| with oldest first.
    std::sort(kept_pages.begin(), kept_pages.end(),
              [](const OfflinePageItem& a, const OfflinePageItem& b) -> bool {
                return a.last_access_time < b.last_access_time;
              });
    size_t kept_pages_size = kept_pages.size();
    size_t pos = 0;
    while (pos < kept_pages_size && space_to_release > 0) {
      space_to_release -= kept_pages.at(pos).file_size;
      page_ids_to_clear->push_back(kept_pages.at(pos).offline_id);
      pos++;
    }
  }
}

OfflinePageStorageManager::ClearMode
OfflinePageStorageManager::ShouldClearPages(
    const ArchiveManager::StorageStats& storage_stats) {
  int64_t total_size = storage_stats.temporary_archives_size;
  int64_t free_space = storage_stats.free_disk_space;
  if (total_size == 0)
    return ClearMode::NOT_NEEDED;

  // If the size of all offline pages is more than limit, or it's larger than a
  // specified percentage of all available storage space on the disk we'll clear
  // all offline pages.
  if (total_size >=
      (total_size + free_space) * constants::kOfflinePageStorageLimit)
    return ClearMode::DEFAULT;
  // If it's been more than the pre-defined interval since the last time we
  // clear the storage, we should clear pages.
  if (last_clear_time_ == base::Time() ||
      clear_time_ - last_clear_time_ >= constants::kClearStorageInterval) {
    return ClearMode::DEFAULT;
  }
  // Otherwise there's no need to clear storage right now.
  return ClearMode::NOT_NEEDED;
}

void OfflinePageStorageManager::ReportStorageUsageUMA(
    const MultipleOfflinePageItemResult& pages) {
  // Only run once per app launch to make the data less noisy.
  // TODO(petewil): Once per day might be better, but would need a new field in
  // the database, and a new async task to get it.
  if (reported_usage_this_launch_)
    return;
  reported_usage_this_launch_ = true;

  // Iterate through all the pages, getting their size, and adding to the proper
  // namespace accumulator.
  std::map<std::string, int64_t> page_sizes;
  for (const OfflinePageItem& item : pages) {
    const std::string& name_space = item.client_id.name_space;
    // Note: if |name_space| is not yet a key in the map a new entry will be
    // properly initialized with the value 0 because |int| is
    // default-constructible (int() == 0).
    page_sizes[name_space] += item.file_size;
  }

  // Report usages for all namespaces known to ClientPolicyController and only
  // for those.
  std::string base_histogram_name = "OfflinePages.ClearStoragePreRunUsage.";
  for (const std::string name_space : policy_controller_->GetAllNamespaces()) {
    int size_in_kib = base::saturated_cast<int>(page_sizes[name_space] / 1024);
    base::UmaHistogramCustomCounts(base_histogram_name + name_space,
                                   size_in_kib, 1, 10000000, 50);
  }
}

bool OfflinePageStorageManager::IsExpired(const OfflinePageItem& page) const {
  const LifetimePolicy& policy =
      policy_controller_->GetPolicy(page.client_id.name_space).lifetime_policy;
  return policy.lifetime_type == LifetimeType::TEMPORARY &&
         clear_time_ - page.last_access_time >= policy.expiration_period;
}

bool OfflinePageStorageManager::IsInProgress() const {
  return clear_time_ > last_clear_time_;
}

}  // namespace offline_pages
