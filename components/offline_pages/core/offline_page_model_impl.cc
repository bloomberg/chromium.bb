// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_model_impl.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model_query.h"
#include "components/offline_pages/core/offline_page_storage_manager.h"
#include "url/gurl.h"

using ArchiverResult = offline_pages::OfflinePageArchiver::ArchiverResult;
using ClearStorageCallback =
    offline_pages::OfflinePageStorageManager::ClearStorageCallback;
using ClearStorageResult =
    offline_pages::OfflinePageStorageManager::ClearStorageResult;

namespace offline_pages {

namespace {

// The delay used to schedule the first clear storage request for storage
// manager after the model is loaded.
const base::TimeDelta kStorageManagerStartingDelay =
    base::TimeDelta::FromSeconds(20);

// Number of times to try to initialize the underlying database.
// TODO(dimich): Replace with a schema that eventually obliterates the database
// if it has permanent damage. Note this DB contains data saved by user, so
// need to be gentle.
const int kInitializeAttemptsMax = 3;

int64_t GenerateOfflineId() {
  return base::RandGenerator(std::numeric_limits<int64_t>::max()) + 1;
}

// The maximum histogram size for the metrics that measure time between views of
// a given page.
const base::TimeDelta kMaxOpenedPageHistogramBucket =
    base::TimeDelta::FromDays(90);

SavePageResult ToSavePageResult(ArchiverResult archiver_result) {
  SavePageResult result;
  switch (archiver_result) {
    case ArchiverResult::SUCCESSFULLY_CREATED:
      result = SavePageResult::SUCCESS;
      break;
    case ArchiverResult::ERROR_DEVICE_FULL:
      result = SavePageResult::DEVICE_FULL;
      break;
    case ArchiverResult::ERROR_CONTENT_UNAVAILABLE:
      result = SavePageResult::CONTENT_UNAVAILABLE;
      break;
    case ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED:
      result = SavePageResult::ARCHIVE_CREATION_FAILED;
      break;
    case ArchiverResult::ERROR_CANCELED:
      result = SavePageResult::CANCELLED;
      break;
    case ArchiverResult::ERROR_SECURITY_CERTIFICATE:
      result = SavePageResult::SECURITY_CERTIFICATE_ERROR;
      break;
    case ArchiverResult::ERROR_ERROR_PAGE:
      result = SavePageResult::ERROR_PAGE;
      break;
    case ArchiverResult::ERROR_INTERSTITIAL_PAGE:
      result = SavePageResult::INTERSTITIAL_PAGE;
    default:
      NOTREACHED();
      result = SavePageResult::CONTENT_UNAVAILABLE;
  }
  return result;
}

std::string AddHistogramSuffix(const ClientId& client_id,
                               const char* histogram_name) {
  if (client_id.name_space.empty()) {
    NOTREACHED();
    return histogram_name;
  }
  std::string adjusted_histogram_name(histogram_name);
  adjusted_histogram_name += ".";
  adjusted_histogram_name += client_id.name_space;
  return adjusted_histogram_name;
}

void ReportStorageHistogramsAfterSave(
    const ArchiveManager::StorageStats& storage_stats) {
  const int kMB = 1024 * 1024;
  int free_disk_space_mb =
      static_cast<int>(storage_stats.free_disk_space / kMB);
  UMA_HISTOGRAM_CUSTOM_COUNTS("OfflinePages.SavePage.FreeSpaceMB",
                              free_disk_space_mb, 1, 500000, 50);

  int total_page_size_mb =
      static_cast<int>(storage_stats.total_archives_size / kMB);
  UMA_HISTOGRAM_COUNTS_10000("OfflinePages.TotalPageSize", total_page_size_mb);
}

void ReportStorageHistogramsAfterDelete(
    const ArchiveManager::StorageStats& storage_stats) {
  const int kMB = 1024 * 1024;
  int free_disk_space_mb =
      static_cast<int>(storage_stats.free_disk_space / kMB);
  UMA_HISTOGRAM_CUSTOM_COUNTS("OfflinePages.DeletePage.FreeSpaceMB",
                              free_disk_space_mb, 1, 500000, 50);

  int total_page_size_mb =
      static_cast<int>(storage_stats.total_archives_size / kMB);
  UMA_HISTOGRAM_COUNTS_10000("OfflinePages.TotalPageSize", total_page_size_mb);

  if (storage_stats.free_disk_space > 0) {
    int percentage_of_free = static_cast<int>(
        1.0 * storage_stats.total_archives_size /
        (storage_stats.total_archives_size + storage_stats.free_disk_space) *
        100);
    UMA_HISTOGRAM_PERCENTAGE(
        "OfflinePages.DeletePage.TotalPageSizeAsPercentageOfFreeSpace",
        percentage_of_free);
  }
}

void ReportSavePageResultHistogramAfterSave(const ClientId& client_id,
                                            SavePageResult result) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      AddHistogramSuffix(client_id, "OfflinePages.SavePageResult"), 1,
      static_cast<int>(SavePageResult::RESULT_COUNT),
      static_cast<int>(SavePageResult::RESULT_COUNT) + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<int>(result));
}

// Goes through the list of offline pages, compiling the following two metrics:
//   - a count of the pages with the same URL
//   - The difference between the |created_before| time and the creation time of
//     the page with the closest creation time before |created_before|.
// Returns true if there was a page that was saved before |created_before| with
// a matching URL.
bool GetMatchingURLCountAndMostRecentCreationTime(
    const std::map<int64_t, OfflinePageItem>& offline_pages,
    std::string name_space,
    const GURL& url,
    base::Time created_before,
    int* matching_url_count,
    base::TimeDelta* most_recent_creation_time) {
  int count = 0;

  // Create a time that is very old, so that any valid time will be newer than
  // it.
  base::Time latest_time;
  bool matching_page = false;

  for (auto& id_page_pair : offline_pages) {
    if (id_page_pair.second.client_id.name_space == name_space &&
        url == id_page_pair.second.url) {
      count++;
      base::Time page_creation_time = id_page_pair.second.creation_time;
      if (page_creation_time < created_before &&
          page_creation_time > latest_time) {
        latest_time = page_creation_time;
        matching_page = true;
      }
    }
  }

  if (matching_url_count != nullptr)
    *matching_url_count = count;
  if (most_recent_creation_time != nullptr && latest_time != base::Time())
    *most_recent_creation_time = created_before - latest_time;

  return matching_page;
}

void ReportPageHistogramAfterSave(
    ClientPolicyController* policy_controller_,
    const std::map<int64_t, OfflinePageItem>& offline_pages,
    const OfflinePageItem& offline_page,
    const base::Time& save_time) {
  DCHECK(policy_controller_);
  // The histogram below is an expansion of the UMA_HISTOGRAM_TIMES
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      AddHistogramSuffix(offline_page.client_id, "OfflinePages.SavePageTime"),
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(10),
      50, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(save_time - offline_page.creation_time);

  // The histogram below is an expansion of the UMA_HISTOGRAM_CUSTOM_COUNTS
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  // Reported as Kb between 1Kb and 10Mb.
  histogram = base::Histogram::FactoryGet(
      AddHistogramSuffix(offline_page.client_id, "OfflinePages.PageSize"), 1,
      10000, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(offline_page.file_size / 1024);

  if (policy_controller_->IsSupportedByDownload(
          offline_page.client_id.name_space)) {
    int matching_url_count;
    base::TimeDelta time_since_most_recent_duplicate;
    if (GetMatchingURLCountAndMostRecentCreationTime(
            offline_pages, offline_page.client_id.name_space, offline_page.url,
            offline_page.creation_time, &matching_url_count,
            &time_since_most_recent_duplicate)) {
      // Using CUSTOM_COUNTS instead of time-oriented histogram to record
      // samples in seconds rather than milliseconds.
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "OfflinePages.DownloadSavedPageTimeSinceDuplicateSaved",
          time_since_most_recent_duplicate.InSeconds(),
          base::TimeDelta::FromSeconds(1).InSeconds(),
          base::TimeDelta::FromDays(7).InSeconds(), 50);
    }
    UMA_HISTOGRAM_CUSTOM_COUNTS("OfflinePages.DownloadSavedPageDuplicateCount",
                                matching_url_count, 1, 20, 10);
  }
}

void ReportPageHistogramsAfterDelete(
    const std::map<int64_t, OfflinePageItem>& offline_pages,
    const std::vector<OfflinePageItem>& deleted_pages,
    const base::Time& delete_time) {
  const int max_minutes = base::TimeDelta::FromDays(365).InMinutes();
  int64_t total_size = 0;

  for (const auto& page : deleted_pages) {
    total_size += page.file_size;
    ClientId client_id = page.client_id;

    if (client_id.name_space == kDownloadNamespace) {
      int remaining_pages_with_url;
      GetMatchingURLCountAndMostRecentCreationTime(
          offline_pages, page.client_id.name_space, page.url, base::Time::Max(),
          &remaining_pages_with_url, nullptr);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "OfflinePages.DownloadDeletedPageDuplicateCount",
          remaining_pages_with_url, 1, 20, 10);
    }

    // The histograms below are an expansion of the UMA_HISTOGRAM_CUSTOM_COUNTS
    // macro adapted to allow for a dynamically suffixed histogram name.
    // Note: The factory creates and owns the histogram.
    base::HistogramBase* histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id, "OfflinePages.PageLifetime"), 1,
        max_minutes, 100, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add((delete_time - page.creation_time).InMinutes());

    histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id,
                           "OfflinePages.DeletePage.TimeSinceLastOpen"),
        1, max_minutes, 100, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add((delete_time - page.last_access_time).InMinutes());

    histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id,
                           "OfflinePages.DeletePage.LastOpenToCreated"),
        1, max_minutes, 100, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add((page.last_access_time - page.creation_time).InMinutes());

    // Reported as Kb between 1Kb and 10Mb.
    histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id, "OfflinePages.DeletePage.PageSize"), 1,
        10000, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(page.file_size / 1024);

    histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id, "OfflinePages.DeletePage.AccessCount"), 1,
        1000000, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(page.access_count);
  }

  if (deleted_pages.size() > 1) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.BatchDelete.Count",
                         static_cast<int32_t>(deleted_pages.size()));
    UMA_HISTOGRAM_MEMORY_KB("OfflinePages.BatchDelete.TotalPageSize",
                            total_size / 1024);
  }
}

void ReportPageHistogramsAfterAccess(const OfflinePageItem& offline_page_item,
                                     const base::Time& access_time) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_CUSTOM_COUNTS
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      AddHistogramSuffix(offline_page_item.client_id,
                         offline_page_item.access_count == 0
                             ? "OfflinePages.FirstOpenSinceCreated"
                             : "OfflinePages.OpenSinceLastOpen"),
      1, kMaxOpenedPageHistogramBucket.InMinutes(), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(
      (access_time - offline_page_item.last_access_time).InMinutes());
}

void ReportInitializationAttemptsSpent(int attempts_spent) {
  UMA_HISTOGRAM_EXACT_LINEAR("OfflinePages.Model.InitAttemptsSpent",
                             attempts_spent, kInitializeAttemptsMax);
}

}  // namespace

// protected
OfflinePageModelImpl::OfflinePageModelImpl()
    : OfflinePageModel(),
      is_loaded_(false),
      testing_clock_(nullptr),
      skip_clearing_original_url_for_testing_(false),
      weak_ptr_factory_(this) {}

OfflinePageModelImpl::OfflinePageModelImpl(
    std::unique_ptr<OfflinePageMetadataStore> store,
    const base::FilePath& archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(std::move(store)),
      archives_dir_(archives_dir),
      is_loaded_(false),
      policy_controller_(new ClientPolicyController()),
      archive_manager_(new ArchiveManager(archives_dir, task_runner)),
      testing_clock_(nullptr),
      skip_clearing_original_url_for_testing_(false),
      weak_ptr_factory_(this) {
  archive_manager_->EnsureArchivesDirCreated(
      base::Bind(&OfflinePageModelImpl::OnEnsureArchivesDirCreatedDone,
                 weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

OfflinePageModelImpl::~OfflinePageModelImpl() {}

void OfflinePageModelImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OfflinePageModelImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OfflinePageModelImpl::SavePage(
    const SavePageParams& save_page_params,
    std::unique_ptr<OfflinePageArchiver> archiver,
    const SavePageCallback& callback) {
  DCHECK(is_loaded_);

  // Skip saving the page that is not intended to be saved, like local file
  // page.
  if (!OfflinePageModel::CanSaveURL(save_page_params.url)) {
    InformSavePageDone(callback, SavePageResult::SKIPPED,
                       save_page_params.client_id, kInvalidOfflineId);
    return;
  }

  // The web contents is not available if archiver is not created and passed.
  if (!archiver.get()) {
    InformSavePageDone(callback, SavePageResult::CONTENT_UNAVAILABLE,
                       save_page_params.client_id, kInvalidOfflineId);
    return;
  }

  // If we already have an offline id, use it.  If not, generate one.
  int64_t offline_id = save_page_params.proposed_offline_id;
  if (offline_id == kInvalidOfflineId)
    offline_id = GenerateOfflineId();

  OfflinePageArchiver::CreateArchiveParams create_archive_params;
  // If the page is being saved in the background, we should try to remove the
  // popup overlay that obstructs viewing the normal content.
  create_archive_params.remove_popup_overlay = save_page_params.is_background;
  create_archive_params.use_page_problem_detectors =
      save_page_params.use_page_problem_detectors;
  archiver->CreateArchive(
      archives_dir_, create_archive_params,
      base::Bind(&OfflinePageModelImpl::OnCreateArchiveDone,
                 weak_ptr_factory_.GetWeakPtr(), save_page_params, offline_id,
                 GetCurrentTime(), callback));
  pending_archivers_.push_back(std::move(archiver));
}

void OfflinePageModelImpl::AddPage(const OfflinePageItem& page,
                                   const AddPageCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModelImpl::AddPageWhenLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), page, callback));
}

void OfflinePageModelImpl::AddPageWhenLoadDone(
    const OfflinePageItem& page,
    const AddPageCallback& callback) {
  store_->AddOfflinePage(
      page, base::Bind(&OfflinePageModelImpl::OnAddPageDone,
                       weak_ptr_factory_.GetWeakPtr(), page, callback));
}

void OfflinePageModelImpl::MarkPageAccessed(int64_t offline_id) {
  RunWhenLoaded(base::Bind(&OfflinePageModelImpl::MarkPageAccessedWhenLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), offline_id));
}

void OfflinePageModelImpl::MarkPageAccessedWhenLoadDone(int64_t offline_id) {
  DCHECK(is_loaded_);

  auto iter = offline_pages_.find(offline_id);
  if (iter == offline_pages_.end())
    return;

  // Make a copy of the cached item and update it. The cached item should only
  // be updated upon the successful store operation.
  OfflinePageItem offline_page_item = iter->second;

  ReportPageHistogramsAfterAccess(offline_page_item, GetCurrentTime());

  offline_page_item.last_access_time = GetCurrentTime();
  offline_page_item.access_count++;

  std::vector<OfflinePageItem> items = {offline_page_item};
  store_->UpdateOfflinePages(
      items, base::Bind(&OfflinePageModelImpl::OnMarkPageAccesseDone,
                        weak_ptr_factory_.GetWeakPtr(), offline_page_item));
}

void OfflinePageModelImpl::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModelImpl::DoDeletePagesByOfflineId,
                           weak_ptr_factory_.GetWeakPtr(), offline_ids,
                           callback));
}

void OfflinePageModelImpl::DoDeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {
  DCHECK(is_loaded_);

  std::vector<base::FilePath> paths_to_delete;
  for (const auto& offline_id : offline_ids) {
    auto iter = offline_pages_.find(offline_id);
    if (iter != offline_pages_.end()) {
      paths_to_delete.push_back(iter->second.file_path);
    }
  }

  // If there're no pages to delete, return early.
  if (paths_to_delete.empty()) {
    InformDeletePageDone(callback, DeletePageResult::SUCCESS);
    return;
  }

  archive_manager_->DeleteMultipleArchives(
      paths_to_delete,
      base::Bind(&OfflinePageModelImpl::OnDeleteArchiveFilesDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_ids, callback));
}

void OfflinePageModelImpl::DeletePagesByClientIds(
    const std::vector<ClientId>& client_ids,
    const DeletePageCallback& callback) {
  OfflinePageModelQueryBuilder builder;
  builder.SetClientIds(OfflinePageModelQuery::Requirement::INCLUDE_MATCHING,
                       client_ids);
  auto delete_pages = base::Bind(&OfflinePageModelImpl::DeletePages,
                                 weak_ptr_factory_.GetWeakPtr(), callback);
  RunWhenLoaded(base::Bind(
      &OfflinePageModelImpl::GetPagesMatchingQueryWhenLoadDone,
      weak_ptr_factory_.GetWeakPtr(),
      base::Passed(builder.Build(GetPolicyController())), delete_pages));
}

void OfflinePageModelImpl::DeletePages(
    const DeletePageCallback& callback,
    const MultipleOfflinePageItemResult& pages) {
  DCHECK(is_loaded_);

  std::vector<int64_t> offline_ids;
  for (auto& page : pages)
    offline_ids.emplace_back(page.offline_id);

  DoDeletePagesByOfflineId(offline_ids, callback);
}

void OfflinePageModelImpl::GetPagesMatchingQuery(
    std::unique_ptr<OfflinePageModelQuery> query,
    const MultipleOfflinePageItemCallback& callback) {
  RunWhenLoaded(base::Bind(
      &OfflinePageModelImpl::GetPagesMatchingQueryWhenLoadDone,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(&query), callback));
}

void OfflinePageModelImpl::GetPagesMatchingQueryWhenLoadDone(
    std::unique_ptr<OfflinePageModelQuery> query,
    const MultipleOfflinePageItemCallback& callback) {
  DCHECK(query);
  DCHECK(is_loaded_);

  MultipleOfflinePageItemResult offline_pages_result;

  for (const auto& id_page_pair : offline_pages_) {
    if (query->Matches(id_page_pair.second))
      offline_pages_result.emplace_back(id_page_pair.second);
  }

  callback.Run(offline_pages_result);
}

void OfflinePageModelImpl::GetPagesByClientIds(
    const std::vector<ClientId>& client_ids,
    const MultipleOfflinePageItemCallback& callback) {
  OfflinePageModelQueryBuilder builder;
  builder.SetClientIds(OfflinePageModelQuery::Requirement::INCLUDE_MATCHING,
                       client_ids);
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetPagesMatchingQueryWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(builder.Build(GetPolicyController())), callback));
}

void OfflinePageModelImpl::DeleteCachedPagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::DoDeleteCachedPagesByURLPredicate,
                 weak_ptr_factory_.GetWeakPtr(), predicate, callback));
}

void OfflinePageModelImpl::DoDeleteCachedPagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {
  DCHECK(is_loaded_);

  std::vector<int64_t> offline_ids;
  for (const auto& id_page_pair : offline_pages_) {
    if (IsRemovedOnCacheReset(id_page_pair.second) &&
        predicate.Run(id_page_pair.second.url)) {
      offline_ids.push_back(id_page_pair.first);
    }
  }
  DoDeletePagesByOfflineId(offline_ids, callback);
}

void OfflinePageModelImpl::CheckPagesExistOffline(
    const std::set<GURL>& urls,
    const CheckPagesExistOfflineCallback& callback) {
  OfflinePageModelQueryBuilder builder;
  builder
      .SetUrls(OfflinePageModelQuery::Requirement::INCLUDE_MATCHING,
               std::vector<GURL>(urls.begin(), urls.end()))
      .RequireRestrictedToOriginalTab(
          OfflinePageModelQueryBuilder::Requirement::EXCLUDE_MATCHING);
  auto pages_to_urls = base::Bind(
      [](const CheckPagesExistOfflineCallback& callback,
         const MultipleOfflinePageItemResult& pages) {
        CheckPagesExistOfflineResult result;
        for (auto& page : pages)
          result.insert(page.url);
        callback.Run(result);
      },
      callback);
  RunWhenLoaded(base::Bind(
      &OfflinePageModelImpl::GetPagesMatchingQueryWhenLoadDone,
      weak_ptr_factory_.GetWeakPtr(),
      base::Passed(builder.Build(GetPolicyController())), pages_to_urls));
}

void OfflinePageModelImpl::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {
  OfflinePageModelQueryBuilder builder;
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetPagesMatchingQueryWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(builder.Build(GetPolicyController())), callback));
}

void OfflinePageModelImpl::GetOfflineIdsForClientId(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetOfflineIdsForClientIdWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), client_id, callback));
}

void OfflinePageModelImpl::GetOfflineIdsForClientIdWhenLoadDone(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) const {
  DCHECK(is_loaded_);
  callback.Run(MaybeGetOfflineIdsForClientId(client_id));
}

const std::vector<int64_t> OfflinePageModelImpl::MaybeGetOfflineIdsForClientId(
    const ClientId& client_id) const {
  DCHECK(is_loaded_);
  std::vector<int64_t> results;

  // We want only all pages, including those marked for deletion.
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.client_id == client_id)
      results.push_back(id_page_pair.second.offline_id);
  }
  return results;
}

void OfflinePageModelImpl::GetPageByOfflineId(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) {
  std::vector<int64_t> query_ids;
  query_ids.emplace_back(offline_id);

  OfflinePageModelQueryBuilder builder;
  builder.SetOfflinePageIds(
      OfflinePageModelQuery::Requirement::INCLUDE_MATCHING, query_ids);

  auto multiple_callback = base::Bind(
      [](const SingleOfflinePageItemCallback& callback,
         const MultipleOfflinePageItemResult& result) {
        DCHECK_LE(result.size(), 1U);
        if (result.empty()) {
          callback.Run(nullptr);
        } else {
          callback.Run(&result[0]);
        }
      },
      callback);

  RunWhenLoaded(base::Bind(
      &OfflinePageModelImpl::GetPagesMatchingQueryWhenLoadDone,
      weak_ptr_factory_.GetWeakPtr(),
      base::Passed(builder.Build(GetPolicyController())), multiple_callback));
}

void OfflinePageModelImpl::GetPagesByURL(
    const GURL& url,
    URLSearchMode url_search_mode,
    const MultipleOfflinePageItemCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetPagesByURLWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), url,
                 url_search_mode, callback));
}

void OfflinePageModelImpl::GetPagesByURLWhenLoadDone(
    const GURL& url,
    URLSearchMode url_search_mode,
    const MultipleOfflinePageItemCallback& callback) const {
  DCHECK(is_loaded_);
  std::vector<OfflinePageItem> result;

  GURL::Replacements remove_params;
  remove_params.ClearRef();

  GURL url_without_fragment =
      url.ReplaceComponents(remove_params);

  for (const auto& id_page_pair : offline_pages_) {
    // First, search by last committed URL with fragment stripped.
    if (url_without_fragment ==
            id_page_pair.second.url.ReplaceComponents(remove_params)) {
      result.push_back(id_page_pair.second);
      continue;
    }
    // Then, search by original request URL if |url_search_mode| wants it.
    // Note that we want to do the exact match with fragment included. This is
    // because original URL is used for redirect purpose and it is always safer
    // to support the exact redirect.
    if (url_search_mode == URLSearchMode::SEARCH_BY_ALL_URLS &&
        url == id_page_pair.second.original_url) {
      result.push_back(id_page_pair.second);
    }
  }

  callback.Run(result);
}

void OfflinePageModelImpl::CheckMetadataConsistency() {
  DCHECK(is_loaded_);

  // Avoid consistency check if disk store couldn't load.
  if (store_->state() != StoreState::LOADED)
    return;

  archive_manager_->GetAllArchives(
      base::Bind(&OfflinePageModelImpl::CheckMetadataConsistencyForArchivePaths,
                 weak_ptr_factory_.GetWeakPtr()));
}

ClientPolicyController* OfflinePageModelImpl::GetPolicyController() {
  return policy_controller_.get();
}

OfflinePageMetadataStore* OfflinePageModelImpl::GetStoreForTesting() {
  return store_.get();
}

OfflinePageStorageManager* OfflinePageModelImpl::GetStorageManager() {
  return storage_manager_.get();
}

bool OfflinePageModelImpl::is_loaded() const {
  return is_loaded_;
}

OfflineEventLogger* OfflinePageModelImpl::GetLogger() {
  return &offline_event_logger_;
}

void OfflinePageModelImpl::OnCreateArchiveDone(
    const SavePageParams& save_page_params,
    int64_t offline_id,
    const base::Time& start_time,
    const SavePageCallback& callback,
    OfflinePageArchiver* archiver,
    ArchiverResult archiver_result,
    const GURL& saved_url,
    const base::FilePath& file_path,
    const base::string16& title,
    int64_t file_size) {
  DeletePendingArchiver(archiver);

  if (archiver_result != ArchiverResult::SUCCESSFULLY_CREATED) {
    SavePageResult result = ToSavePageResult(archiver_result);
    InformSavePageDone(
        callback, result, save_page_params.client_id, offline_id);
    return;
  }

  if (save_page_params.url != saved_url) {
    DVLOG(1) << "Saved URL does not match requested URL.";
    InformSavePageDone(callback, SavePageResult::ARCHIVE_CREATION_FAILED,
                       save_page_params.client_id, offline_id);
    return;
  }

  OfflinePageItem offline_page(saved_url, offline_id,
                               save_page_params.client_id, file_path, file_size,
                               start_time);
  offline_page.title = title;
  // Don't record the original URL if it is identical to the final URL. This is
  // because some websites might route the redirect finally back to itself upon
  // the completion of certain action, i.e., authentication, in the middle.
  if (skip_clearing_original_url_for_testing_ ||
      save_page_params.original_url != offline_page.url) {
    offline_page.original_url = save_page_params.original_url;
  }
  offline_page.request_origin = save_page_params.request_origin;
  AddPageWhenLoadDone(
      offline_page,
      base::Bind(&OfflinePageModelImpl::OnAddSavedPageDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_page, callback));
}

void OfflinePageModelImpl::OnAddPageDone(const OfflinePageItem& offline_page,
                                         const AddPageCallback& callback,
                                         ItemActionStatus status) {
  AddPageResult result;
  if (status == ItemActionStatus::SUCCESS) {
    offline_pages_[offline_page.offline_id] = offline_page;
    result = AddPageResult::SUCCESS;
    ReportPageHistogramAfterSave(policy_controller_.get(), offline_pages_,
                                 offline_page, GetCurrentTime());
    offline_event_logger_.RecordPageSaved(offline_page.client_id.name_space,
                                          offline_page.url.spec(),
                                          offline_page.offline_id);
  } else if (status == ItemActionStatus::ALREADY_EXISTS) {
    // Remove the orphaned archive.  No callback necessary.
    archive_manager_->DeleteArchive(offline_page.file_path,
                                    base::Bind([](bool) {}));
    result = AddPageResult::ALREADY_EXISTS;
  } else {
    result = AddPageResult::STORE_FAILURE;
  }

  callback.Run(result, offline_page.offline_id);

  // We don't want to notify observers if the add failed.
  if (status == ItemActionStatus::SUCCESS) {
    for (Observer& observer : observers_)
      observer.OfflinePageAdded(this, offline_page);
  }
}

void OfflinePageModelImpl::OnAddSavedPageDone(
    const OfflinePageItem& offline_page,
    const SavePageCallback& callback,
    AddPageResult add_result,
    int64_t offline_id) {
  SavePageResult save_result;
  if (add_result == AddPageResult::SUCCESS) {
    save_result = SavePageResult::SUCCESS;
  } else if (add_result == AddPageResult::ALREADY_EXISTS) {
    save_result = SavePageResult::ALREADY_EXISTS;
  } else if (add_result == AddPageResult::STORE_FAILURE) {
    save_result = SavePageResult::STORE_FAILURE;
  } else {
    NOTREACHED();
    save_result = SavePageResult::STORE_FAILURE;
  }
  InformSavePageDone(callback, save_result, offline_page.client_id, offline_id);
  if (save_result == SavePageResult::SUCCESS) {
    DeleteExistingPagesWithSameURL(offline_page);
  } else {
    PostClearStorageIfNeededTask(false /* delayed */);
  }
}

void OfflinePageModelImpl::OnMarkPageAccesseDone(
    const OfflinePageItem& offline_page_item,
    std::unique_ptr<OfflinePagesUpdateResult> result) {
  // Update the item in the cache only upon success.
  if (result->updated_items.size() > 0)
    offline_pages_[offline_page_item.offline_id] = offline_page_item;

  // No need to fire OfflinePageModelChanged event since updating access info
  // should not have any impact to the UI.
}

void OfflinePageModelImpl::OnEnsureArchivesDirCreatedDone(
    const base::TimeTicks& start_time) {
  UMA_HISTOGRAM_TIMES("OfflinePages.Model.ArchiveDirCreationTime",
                      base::TimeTicks::Now() - start_time);

  store_->Initialize(base::Bind(&OfflinePageModelImpl::OnStoreInitialized,
                                weak_ptr_factory_.GetWeakPtr(), start_time, 0));
}

void OfflinePageModelImpl::OnStoreInitialized(const base::TimeTicks& start_time,
                                              int init_attempts_spent,
                                              bool success) {
  init_attempts_spent++;

  if (success) {
    DCHECK_EQ(store_->state(), StoreState::LOADED);
    ReportInitializationAttemptsSpent(init_attempts_spent);
    store_->GetOfflinePages(
        base::Bind(&OfflinePageModelImpl::OnInitialGetOfflinePagesDone,
                   weak_ptr_factory_.GetWeakPtr(), start_time));
    return;
  }

  DCHECK_EQ(store_->state(), StoreState::FAILED_LOADING);
  // If there are no more init attempts left, stop here.
  if (init_attempts_spent >= kInitializeAttemptsMax) {
    FinalizeModelLoad();
    return;
  }

  // The DB failed to load. If this is a transient condition (locks not
  // yet released etc) chances are that a retry with a delay will succeed.
  const base::TimeDelta delay = base::TimeDelta::FromMilliseconds(100);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&OfflinePageModelImpl::RetryDbInitialization,
                 weak_ptr_factory_.GetWeakPtr(), start_time,
                 init_attempts_spent),
      delay);
}

void OfflinePageModelImpl::RetryDbInitialization(
    const base::TimeTicks& start_time,
    int init_attempts_spent) {
  store_->Initialize(base::Bind(&OfflinePageModelImpl::OnStoreInitialized,
                                weak_ptr_factory_.GetWeakPtr(), start_time,
                                init_attempts_spent));
}

void OfflinePageModelImpl::OnInitialGetOfflinePagesDone(
    const base::TimeTicks& start_time,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK(!is_loaded_);

  UMA_HISTOGRAM_TIMES("OfflinePages.Model.ConstructionToLoadedEventTime",
                      base::TimeTicks::Now() - start_time);

  CacheLoadedData(offline_pages);
  FinalizeModelLoad();

  // Ensure necessary cleanup operations are started.
  CheckMetadataConsistency();
}

void OfflinePageModelImpl::FinalizeModelLoad() {
  is_loaded_ = true;

  // All actions below are meant to be taken regardless of successful load of
  // the store.

  UMA_HISTOGRAM_BOOLEAN("OfflinePages.Model.FinalLoadSuccessful",
                        store_->state() == StoreState::LOADED);

  // Inform observers the load is done.
  for (Observer& observer : observers_)
    observer.OfflinePageModelLoaded(this);

  // Run all the delayed tasks.
  for (const auto& delayed_task : delayed_tasks_)
    delayed_task.Run();
  delayed_tasks_.clear();

  // Clear storage.
  PostClearStorageIfNeededTask(true /* delayed */);
}

void OfflinePageModelImpl::InformSavePageDone(const SavePageCallback& callback,
                                              SavePageResult result,
                                              const ClientId& client_id,
                                              int64_t offline_id) {
  ReportSavePageResultHistogramAfterSave(client_id, result);
  archive_manager_->GetStorageStats(
      base::Bind(&ReportStorageHistogramsAfterSave));
  // No need to pass in a callback, since if the archive manager fails every
  // time when creating an archive directory, there's nothing else to do other
  // than fail every attempt to save a page.
  if (result == SavePageResult::ARCHIVE_CREATION_FAILED)
    archive_manager_->EnsureArchivesDirCreated(base::Bind([]() {}));
  callback.Run(result, offline_id);
}

void OfflinePageModelImpl::DeleteExistingPagesWithSameURL(
    const OfflinePageItem& offline_page) {
  // Remove existing pages generated by the same policy and with same url.
  size_t pages_allowed =
      policy_controller_->GetPolicy(offline_page.client_id.name_space)
          .pages_allowed_per_url;
  if (pages_allowed == kUnlimitedPages)
    return;
  GetPagesByURL(
      offline_page.url,
      URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
      base::Bind(&OfflinePageModelImpl::OnPagesFoundWithSameURL,
                 weak_ptr_factory_.GetWeakPtr(), offline_page, pages_allowed));
}

void OfflinePageModelImpl::OnPagesFoundWithSameURL(
    const OfflinePageItem& offline_page,
    size_t pages_allowed,
    const MultipleOfflinePageItemResult& items) {
  std::vector<OfflinePageItem> pages_to_delete;
  for (const auto& item : items) {
    if (item.offline_id != offline_page.offline_id &&
        item.client_id.name_space == offline_page.client_id.name_space) {
      pages_to_delete.push_back(item);
    }
  }
  // Only keep |pages_allowed|-1 of most fresh pages and delete others, by
  // sorting pages with the oldest  ones first and resize the vector.
  if (pages_to_delete.size() >= pages_allowed) {
    sort(pages_to_delete.begin(), pages_to_delete.end(),
         [](const OfflinePageItem& a, const OfflinePageItem& b) -> bool {
           return a.last_access_time < b.last_access_time;
         });
    pages_to_delete.resize(pages_to_delete.size() - pages_allowed + 1);
  }
  std::vector<int64_t> page_ids_to_delete;
  for (const auto& item : pages_to_delete)
    page_ids_to_delete.push_back(item.offline_id);
  DeletePagesByOfflineId(
      page_ids_to_delete,
      base::Bind(&OfflinePageModelImpl::OnDeleteOldPagesWithSameURL,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageModelImpl::OnDeleteOldPagesWithSameURL(
    DeletePageResult result) {
  PostClearStorageIfNeededTask(false /* delayed */);
}

void OfflinePageModelImpl::DeletePendingArchiver(
    OfflinePageArchiver* archiver) {
  pending_archivers_.erase(
      std::find_if(pending_archivers_.begin(), pending_archivers_.end(),
                   [archiver](const std::unique_ptr<OfflinePageArchiver>& a) {
                     return a.get() == archiver;
                   }));
}

void OfflinePageModelImpl::OnDeleteArchiveFilesDone(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback,
    bool success) {
  if (!success) {
    InformDeletePageDone(callback, DeletePageResult::DEVICE_FAILURE);
    return;
  }

  store_->RemoveOfflinePages(
      offline_ids, base::Bind(&OfflinePageModelImpl::OnRemoveOfflinePagesDone,
                              weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageModelImpl::OnRemoveOfflinePagesDone(
    const DeletePageCallback& callback,
    std::unique_ptr<OfflinePagesUpdateResult> result) {
  ReportPageHistogramsAfterDelete(offline_pages_, result->updated_items,
                                  GetCurrentTime());

  // This part of the loop is explicitly broken out, as it should be gone in
  // fully asynchronous code.
  for (const auto& page : result->updated_items) {
    int64_t offline_id = page.offline_id;
    offline_event_logger_.RecordPageDeleted(offline_id);
    auto iter = offline_pages_.find(offline_id);
    if (iter == offline_pages_.end())
      continue;
    offline_pages_.erase(iter);
  }

  for (const auto& page : result->updated_items) {
    DeletedPageInfo info(page.offline_id, page.client_id, page.request_origin);
    for (Observer& observer : observers_)
      observer.OfflinePageDeleted(info);
  }

  DeletePageResult delete_result;
  if (result->store_state == StoreState::LOADED)
    delete_result = DeletePageResult::SUCCESS;
  else
    delete_result = DeletePageResult::STORE_FAILURE;

  InformDeletePageDone(callback, delete_result);
}

void OfflinePageModelImpl::InformDeletePageDone(
    const DeletePageCallback& callback,
    DeletePageResult result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.DeletePageResult",
                            static_cast<int>(result),
                            static_cast<int>(DeletePageResult::RESULT_COUNT));
  archive_manager_->GetStorageStats(
      base::Bind(&ReportStorageHistogramsAfterDelete));
  if (!callback.is_null())
    callback.Run(result);
}

void OfflinePageModelImpl::CheckMetadataConsistencyForArchivePaths(
    const std::set<base::FilePath>& archive_paths) {
  DeletePagesMissingArchiveFile(archive_paths);
  DeleteOrphanedArchives(archive_paths);
}

void OfflinePageModelImpl::DeletePagesMissingArchiveFile(
    const std::set<base::FilePath>& archive_paths) {
  std::vector<int64_t> ids_of_pages_missing_archive_file;
  for (const auto& id_page_pair : offline_pages_) {
    if (archive_paths.count(id_page_pair.second.file_path) == 0UL)
      ids_of_pages_missing_archive_file.push_back(id_page_pair.first);
  }

  if (ids_of_pages_missing_archive_file.empty())
    return;

  DeletePagesByOfflineId(
      ids_of_pages_missing_archive_file,
      base::Bind(&OfflinePageModelImpl::OnDeletePagesMissingArchiveFileDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 ids_of_pages_missing_archive_file));
}

void OfflinePageModelImpl::OnDeletePagesMissingArchiveFileDone(
    const std::vector<int64_t>& offline_ids,
    DeletePageResult result) {
  UMA_HISTOGRAM_COUNTS("OfflinePages.Consistency.PagesMissingArchiveFileCount",
                       static_cast<int32_t>(offline_ids.size()));
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Consistency.DeletePagesMissingArchiveFileResult",
      static_cast<int>(result),
      static_cast<int>(DeletePageResult::RESULT_COUNT));
}

void OfflinePageModelImpl::DeleteOrphanedArchives(
    const std::set<base::FilePath>& archive_paths) {
  // Archives are considered orphaned unless they are pointed to by some pages.
  std::set<base::FilePath> orphaned_archive_set(archive_paths);
  for (const auto& id_page_pair : offline_pages_)
    orphaned_archive_set.erase(id_page_pair.second.file_path);

  if (orphaned_archive_set.empty())
    return;

  std::vector<base::FilePath> orphaned_archives(orphaned_archive_set.begin(),
                                                orphaned_archive_set.end());
  archive_manager_->DeleteMultipleArchives(
      orphaned_archives,
      base::Bind(&OfflinePageModelImpl::OnDeleteOrphanedArchivesDone,
                 weak_ptr_factory_.GetWeakPtr(), orphaned_archives));
}

void OfflinePageModelImpl::OnDeleteOrphanedArchivesDone(
    const std::vector<base::FilePath>& archives,
    bool success) {
  UMA_HISTOGRAM_COUNTS("OfflinePages.Consistency.OrphanedArchivesCount",
                       static_cast<int32_t>(archives.size()));
  UMA_HISTOGRAM_BOOLEAN("OfflinePages.Consistency.DeleteOrphanedArchivesResult",
                        success);
}

void OfflinePageModelImpl::CacheLoadedData(
    const std::vector<OfflinePageItem>& offline_pages) {
  offline_pages_.clear();
  for (const auto& offline_page : offline_pages)
    offline_pages_[offline_page.offline_id] = offline_page;
}

void OfflinePageModelImpl::ClearStorageIfNeeded(
    const ClearStorageCallback& callback) {
  // Create Storage Manager if necessary.
  if (!storage_manager_) {
    storage_manager_.reset(new OfflinePageStorageManager(
        this, GetPolicyController(), archive_manager_.get()));
  }
  storage_manager_->ClearPagesIfNeeded(callback);
}

void OfflinePageModelImpl::OnStorageCleared(size_t deleted_page_count,
                                            ClearStorageResult result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.ClearStorageResult",
                            static_cast<int>(result),
                            static_cast<int>(ClearStorageResult::RESULT_COUNT));
  if (deleted_page_count > 0) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.ClearStorageBatchSize",
                         static_cast<int32_t>(deleted_page_count));
  }
}

void OfflinePageModelImpl::PostClearStorageIfNeededTask(bool delayed) {
  base::TimeDelta delay =
      delayed ? kStorageManagerStartingDelay : base::TimeDelta();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&OfflinePageModelImpl::ClearStorageIfNeeded,
                            weak_ptr_factory_.GetWeakPtr(),
                            base::Bind(&OfflinePageModelImpl::OnStorageCleared,
                                       weak_ptr_factory_.GetWeakPtr())),
      delay);
}

bool OfflinePageModelImpl::IsRemovedOnCacheReset(
    const OfflinePageItem& offline_page) const {
  return policy_controller_->IsRemovedOnCacheReset(
      offline_page.client_id.name_space);
}

void OfflinePageModelImpl::RunWhenLoaded(const base::Closure& task) {
  if (!is_loaded_) {
    delayed_tasks_.push_back(task);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
}

base::Time OfflinePageModelImpl::GetCurrentTime() const {
  return testing_clock_ ? testing_clock_->Now() : base::Time::Now();
}

}  // namespace offline_pages
