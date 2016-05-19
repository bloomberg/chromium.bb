// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/archive_manager.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_storage_manager.h"
#include "components/offline_pages/proto/offline_pages.pb.h"
#include "url/gurl.h"

using ArchiverResult = offline_pages::OfflinePageArchiver::ArchiverResult;

namespace offline_pages {

namespace {

// This enum is used in an UMA histogram. Hence the entries here shouldn't
// be deleted or re-ordered and new ones should be added to the end.
enum ClearAllStatus {
  CLEAR_ALL_SUCCEEDED,
  STORE_RESET_FAILED,
  STORE_RELOAD_FAILED,

  // NOTE: always keep this entry at the end.
  CLEAR_ALL_STATUS_COUNT
};

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

}  // namespace

// static
bool OfflinePageModel::CanSavePage(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

// protected
OfflinePageModel::OfflinePageModel()
    : is_loaded_(false), weak_ptr_factory_(this) {}

OfflinePageModel::OfflinePageModel(
    std::unique_ptr<OfflinePageMetadataStore> store,
    const base::FilePath& archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(std::move(store)),
      archives_dir_(archives_dir),
      is_loaded_(false),
      policy_controller_(new ClientPolicyController()),
      archive_manager_(new ArchiveManager(archives_dir, task_runner)),
      weak_ptr_factory_(this) {
  archive_manager_->EnsureArchivesDirCreated(
      base::Bind(&OfflinePageModel::OnEnsureArchivesDirCreatedDone,
                 weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

OfflinePageModel::~OfflinePageModel() {
}

void OfflinePageModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OfflinePageModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OfflinePageModel::SavePage(const GURL& url,
                                const ClientId& client_id,
                                std::unique_ptr<OfflinePageArchiver> archiver,
                                const SavePageCallback& callback) {
  DCHECK(is_loaded_);

  // Skip saving the page that is not intended to be saved, like local file
  // page.
  if (url.is_valid() && !CanSavePage(url)) {
    InformSavePageDone(callback, SavePageResult::SKIPPED, client_id,
                       kInvalidOfflineId);
    return;
  }

  // The web contents is not available if archiver is not created and passed.
  if (!archiver.get()) {
    InformSavePageDone(callback, SavePageResult::CONTENT_UNAVAILABLE, client_id,
                       kInvalidOfflineId);
    return;
  }

  int64_t offline_id = GenerateOfflineId();

  archiver->CreateArchive(
      archives_dir_,
      offline_id,
      base::Bind(&OfflinePageModel::OnCreateArchiveDone,
                 weak_ptr_factory_.GetWeakPtr(), url, offline_id,
                 client_id, base::Time::Now(), callback));
  pending_archivers_.push_back(std::move(archiver));
}

void OfflinePageModel::MarkPageAccessed(int64_t offline_id) {
  DCHECK(is_loaded_);
  auto iter = offline_pages_.find(offline_id);
  if (iter == offline_pages_.end())
    return;

  // Make a copy of the cached item and update it. The cached item should only
  // be updated upon the successful store operation.
  OfflinePageItem offline_page_item = iter->second;

  base::Time now = base::Time::Now();
  base::TimeDelta time_since_last_accessed =
      now - offline_page_item.last_access_time;

  // When the access account is still zero, the page is opened for the first
  // time since its creation.
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      AddHistogramSuffix(
          offline_page_item.client_id,
          (offline_page_item.access_count == 0) ?
              "OfflinePages.FirstOpenSinceCreated" :
              "OfflinePages.OpenSinceLastOpen").c_str(),
      time_since_last_accessed.InMinutes(), 1,
      kMaxOpenedPageHistogramBucket.InMinutes(), 50);

  offline_page_item.last_access_time = now;
  offline_page_item.access_count++;

  store_->AddOrUpdateOfflinePage(
      offline_page_item,
      base::Bind(&OfflinePageModel::OnMarkPageAccesseDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_page_item));
}

void OfflinePageModel::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {
  if (!is_loaded_) {
    delayed_tasks_.push_back(
        base::Bind(&OfflinePageModel::DoDeletePagesByOfflineId,
                   weak_ptr_factory_.GetWeakPtr(), offline_ids, callback));

    return;
  }
  DoDeletePagesByOfflineId(offline_ids, callback);
}

void OfflinePageModel::DoDeletePagesByOfflineId(
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

  if (paths_to_delete.empty()) {
    InformDeletePageDone(callback, DeletePageResult::NOT_FOUND);
    return;
  }

  archive_manager_->DeleteMultipleArchives(
      paths_to_delete,
      base::Bind(&OfflinePageModel::OnDeleteArchiveFilesDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_ids, callback));
}

void OfflinePageModel::ClearAll(const base::Closure& callback) {
  DCHECK(is_loaded_);

  std::vector<int64_t> offline_ids;
  for (const auto& id_page_pair : offline_pages_)
    offline_ids.push_back(id_page_pair.first);
  DeletePagesByOfflineId(
      offline_ids,
      base::Bind(&OfflinePageModel::OnRemoveAllFilesDoneForClearAll,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageModel::DeletePagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {
  if (!is_loaded_) {
    delayed_tasks_.push_back(
        base::Bind(&OfflinePageModel::DoDeletePagesByURLPredicate,
                   weak_ptr_factory_.GetWeakPtr(), predicate, callback));

    return;
  }
  DoDeletePagesByURLPredicate(predicate, callback);
}

void OfflinePageModel::DoDeletePagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {
  DCHECK(is_loaded_);

  std::vector<int64_t> offline_ids;
  for (const auto& id_page_pair : offline_pages_) {
    if (predicate.Run(id_page_pair.second.url))
      offline_ids.push_back(id_page_pair.first);
  }
  DoDeletePagesByOfflineId(offline_ids, callback);
}

void OfflinePageModel::HasPages(const std::string& name_space,
                                const HasPagesCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModel::HasPagesAfterLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), name_space,
                           callback));
}

void OfflinePageModel::HasPagesAfterLoadDone(
    const std::string& name_space,
    const HasPagesCallback& callback) const {
  DCHECK(is_loaded_);

  bool has_pages = false;

  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.client_id.name_space == name_space) {
      has_pages = true;
      break;
    }
  }

  callback.Run(has_pages);
}

void OfflinePageModel::CheckPagesExistOffline(
    const std::set<GURL>& urls,
    const CheckPagesExistOfflineCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModel::CheckPagesExistOfflineAfterLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), urls, callback));
}

void OfflinePageModel::CheckPagesExistOfflineAfterLoadDone(
    const std::set<GURL>& urls,
    const CheckPagesExistOfflineCallback& callback) {
  DCHECK(is_loaded_);
  CheckPagesExistOfflineResult result;
  for (const auto& id_page_pair : offline_pages_) {
    auto iter = urls.find(id_page_pair.second.url);
    if (iter != urls.end())
      result.insert(*iter);
  }
  callback.Run(result);
}

void OfflinePageModel::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModel::GetAllPagesAfterLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageModel::GetAllPagesAfterLoadDone(
    const MultipleOfflinePageItemCallback& callback) {
  DCHECK(is_loaded_);

  MultipleOfflinePageItemResult offline_pages;
  for (const auto& id_page_pair : offline_pages_)
    offline_pages.push_back(id_page_pair.second);

  callback.Run(offline_pages);
}

void OfflinePageModel::GetOfflineIdsForClientId(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModel::GetOfflineIdsForClientIdWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), client_id, callback));
}

void OfflinePageModel::GetOfflineIdsForClientIdWhenLoadDone(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) const {
  callback.Run(MaybeGetOfflineIdsForClientId(client_id));
}

const std::vector<int64_t> OfflinePageModel::MaybeGetOfflineIdsForClientId(
    const ClientId& client_id) const {
  DCHECK(is_loaded_);
  std::vector<int64_t> results;

  // We want only all pages, including those marked for deletion.
  // TODO(bburns): actually use an index rather than linear scan.
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.client_id == client_id)
      results.push_back(id_page_pair.second.offline_id);
  }
  return results;
}

void OfflinePageModel::GetPageByOfflineId(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModel::GetPageByOfflineIdWhenLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), offline_id,
                           callback));
}

void OfflinePageModel::GetPageByOfflineIdWhenLoadDone(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) const {
  SingleOfflinePageItemResult result;
  const OfflinePageItem* match = MaybeGetPageByOfflineId(offline_id);
  if (match != nullptr)
    result = *match;
  callback.Run(result);
}

const OfflinePageItem* OfflinePageModel::MaybeGetPageByOfflineId(
    int64_t offline_id) const {
  const auto iter = offline_pages_.find(offline_id);
  return iter != offline_pages_.end() ? &(iter->second) : nullptr;
}

void OfflinePageModel::GetPageByOfflineURL(
    const GURL& offline_url,
    const SingleOfflinePageItemCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModel::GetPageByOfflineURLWhenLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), offline_url,
                           callback));
}

void OfflinePageModel::GetPageByOfflineURLWhenLoadDone(
    const GURL& offline_url,
    const SingleOfflinePageItemCallback& callback) const {
  base::Optional<OfflinePageItem> result;

  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.GetOfflineURL() == offline_url) {
      callback.Run(base::make_optional(id_page_pair.second));
      return;
    }
  }

  callback.Run(base::nullopt);
}

const OfflinePageItem* OfflinePageModel::MaybeGetPageByOfflineURL(
    const GURL& offline_url) const {
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.GetOfflineURL() == offline_url)
      return &(id_page_pair.second);
  }
  return nullptr;
}

void OfflinePageModel::GetBestPageForOnlineURL(
    const GURL& online_url,
    const SingleOfflinePageItemCallback callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModel::GetBestPageForOnlineURLWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), online_url, callback));
}

void OfflinePageModel::GetBestPageForOnlineURLWhenLoadDone(
    const GURL& online_url,
    const SingleOfflinePageItemCallback& callback) const {
  SingleOfflinePageItemResult result;

  const OfflinePageItem* best_page = MaybeGetBestPageForOnlineURL(online_url);
  if (best_page != nullptr)
    result = *best_page;

  callback.Run(result);
}

void OfflinePageModel::GetPagesByOnlineURL(
    const GURL& online_url,
    const MultipleOfflinePageItemCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModel::GetPagesByOnlineURLWhenLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), online_url,
                           callback));
}

void OfflinePageModel::GetPagesByOnlineURLWhenLoadDone(
    const GURL& online_url,
    const MultipleOfflinePageItemCallback& callback) const {
  std::vector<OfflinePageItem> result;

  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.url == online_url)
      result.push_back(id_page_pair.second);
  }

  callback.Run(result);
}

const OfflinePageItem* OfflinePageModel::MaybeGetBestPageForOnlineURL(
    const GURL& online_url) const {
  const OfflinePageItem* result = nullptr;
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.url == online_url) {
      if (!result || id_page_pair.second.creation_time > result->creation_time)
        result = &(id_page_pair.second);
    }
  }
  return result;
}

void OfflinePageModel::CheckForExternalFileDeletion() {
  DCHECK(is_loaded_);

  archive_manager_->GetAllArchives(
      base::Bind(&OfflinePageModel::ScanForMissingArchiveFiles,
                 weak_ptr_factory_.GetWeakPtr()));
}

ClientPolicyController* OfflinePageModel::GetPolicyController() {
  return policy_controller_.get();
}

OfflinePageMetadataStore* OfflinePageModel::GetStoreForTesting() {
  return store_.get();
}

OfflinePageStorageManager* OfflinePageModel::GetStorageManager() {
  return storage_manager_.get();
}

bool OfflinePageModel::is_loaded() const {
  return is_loaded_;
}

void OfflinePageModel::OnCreateArchiveDone(const GURL& requested_url,
                                           int64_t offline_id,
                                           const ClientId& client_id,
                                           const base::Time& start_time,
                                           const SavePageCallback& callback,
                                           OfflinePageArchiver* archiver,
                                           ArchiverResult archiver_result,
                                           const GURL& url,
                                           const base::FilePath& file_path,
                                           int64_t file_size) {
  if (requested_url != url) {
    DVLOG(1) << "Saved URL does not match requested URL.";
    // TODO(fgorski): We have created an archive for a wrong URL. It should be
    // deleted from here, once archiver has the right functionality.
    InformSavePageDone(callback, SavePageResult::ARCHIVE_CREATION_FAILED,
                       client_id, offline_id);
    DeletePendingArchiver(archiver);
    return;
  }

  if (archiver_result != ArchiverResult::SUCCESSFULLY_CREATED) {
    SavePageResult result = ToSavePageResult(archiver_result);
    InformSavePageDone(callback, result, client_id, offline_id);
    DeletePendingArchiver(archiver);
    return;
  }
  OfflinePageItem offline_page_item(url, offline_id, client_id, file_path,
                                    file_size, start_time);
  store_->AddOrUpdateOfflinePage(
      offline_page_item,
      base::Bind(&OfflinePageModel::OnAddOfflinePageDone,
                 weak_ptr_factory_.GetWeakPtr(), archiver, callback,
                 offline_page_item));
}

void OfflinePageModel::OnAddOfflinePageDone(OfflinePageArchiver* archiver,
                                            const SavePageCallback& callback,
                                            const OfflinePageItem& offline_page,
                                            bool success) {
  SavePageResult result;
  if (success) {
    offline_pages_[offline_page.offline_id] = offline_page;
    result = SavePageResult::SUCCESS;
    UMA_HISTOGRAM_TIMES(
        AddHistogramSuffix(
            offline_page.client_id, "OfflinePages.SavePageTime").c_str(),
        base::Time::Now() - offline_page.creation_time);
    // 50 buckets capped between 1Kb and 10Mb.
    UMA_HISTOGRAM_COUNTS_10000(AddHistogramSuffix(
        offline_page.client_id, "OfflinePages.PageSize").c_str(),
        offline_page.file_size / 1024);
  } else {
    result = SavePageResult::STORE_FAILURE;
  }
  InformSavePageDone(callback, result, offline_page.client_id,
                     offline_page.offline_id);
  DeletePendingArchiver(archiver);

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelChanged(this));
}

void OfflinePageModel::OnMarkPageAccesseDone(
    const OfflinePageItem& offline_page_item, bool success) {
  // Update the item in the cache only upon success.
  if (success)
    offline_pages_[offline_page_item.offline_id] = offline_page_item;

  // No need to fire OfflinePageModelChanged event since updating access info
  // should not have any impact to the UI.
}

void OfflinePageModel::OnEnsureArchivesDirCreatedDone(
    const base::TimeTicks& start_time) {
  UMA_HISTOGRAM_TIMES("OfflinePages.Model.ArchiveDirCreationTime",
                      base::TimeTicks::Now() - start_time);

  store_->Load(base::Bind(&OfflinePageModel::OnLoadDone,
                          weak_ptr_factory_.GetWeakPtr(), start_time));
}

void OfflinePageModel::OnLoadDone(
    const base::TimeTicks& start_time,
    OfflinePageMetadataStore::LoadStatus load_status,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK(!is_loaded_);
  is_loaded_ = true;

  // TODO(jianli): rebuild the store upon failure.

  if (load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED)
    CacheLoadedData(offline_pages);

  UMA_HISTOGRAM_TIMES("OfflinePages.Model.ConstructionToLoadedEventTime",
                      base::TimeTicks::Now() - start_time);

  // Create Storage Manager.
  storage_manager_.reset(
      new OfflinePageStorageManager(this, GetPolicyController()));

  // Run all the delayed tasks.
  for (const auto& delayed_task : delayed_tasks_)
    delayed_task.Run();
  delayed_tasks_.clear();

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelLoaded(this));

  CheckForExternalFileDeletion();
}

void OfflinePageModel::InformSavePageDone(const SavePageCallback& callback,
                                          SavePageResult result,
                                          const ClientId& client_id,
                                          int64_t offline_id) {
  UMA_HISTOGRAM_ENUMERATION(
      AddHistogramSuffix(client_id, "OfflinePages.SavePageResult").c_str(),
      static_cast<int>(result),
      static_cast<int>(SavePageResult::RESULT_COUNT));
  archive_manager_->GetStorageStats(
      base::Bind(&ReportStorageHistogramsAfterSave));
  callback.Run(result, offline_id);
}

void OfflinePageModel::DeletePendingArchiver(OfflinePageArchiver* archiver) {
  pending_archivers_.erase(std::find(
      pending_archivers_.begin(), pending_archivers_.end(), archiver));
}

void OfflinePageModel::OnDeleteArchiveFilesDone(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback,
    bool success) {
  if (!success) {
    InformDeletePageDone(callback, DeletePageResult::DEVICE_FAILURE);
    return;
  }

  store_->RemoveOfflinePages(
      offline_ids,
      base::Bind(&OfflinePageModel::OnRemoveOfflinePagesDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_ids, callback));
}

void OfflinePageModel::OnRemoveOfflinePagesDone(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback,
    bool success) {
  // Delete the offline page from the in memory cache regardless of success in
  // store.
  base::Time now = base::Time::Now();
  int64_t total_size = 0;
  for (int64_t offline_id : offline_ids) {
    auto iter = offline_pages_.find(offline_id);
    if (iter == offline_pages_.end())
      continue;
    total_size += iter->second.file_size;
    ClientId client_id = iter->second.client_id;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        AddHistogramSuffix(client_id, "OfflinePages.PageLifetime").c_str(),
        (now - iter->second.creation_time).InMinutes(),
        1,
        base::TimeDelta::FromDays(365).InMinutes(),
        100);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        AddHistogramSuffix(
            client_id, "OfflinePages.DeletePage.TimeSinceLastOpen").c_str(),
        (now - iter->second.last_access_time).InMinutes(),
        1,
        base::TimeDelta::FromDays(365).InMinutes(),
        100);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        AddHistogramSuffix(
            client_id, "OfflinePages.DeletePage.LastOpenToCreated").c_str(),
        (iter->second.last_access_time - iter->second.creation_time).
            InMinutes(),
        1,
        base::TimeDelta::FromDays(365).InMinutes(),
        100);
    UMA_HISTOGRAM_MEMORY_KB(
        AddHistogramSuffix(
            client_id, "OfflinePages.DeletePage.PageSize").c_str(),
        iter->second.file_size / 1024);
    UMA_HISTOGRAM_COUNTS(
        AddHistogramSuffix(
            client_id, "OfflinePages.DeletePage.AccessCount").c_str(),
        iter->second.access_count);
    FOR_EACH_OBSERVER(
        Observer, observers_,
        OfflinePageDeleted(iter->second.offline_id, iter->second.client_id));
    offline_pages_.erase(iter);
  }
  if (offline_ids.size() > 1) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.BatchDelete.Count", offline_ids.size());
    UMA_HISTOGRAM_MEMORY_KB(
        "OfflinePages.BatchDelete.TotalPageSize", total_size / 1024);
  }
  // Deleting multiple pages always succeeds when it gets to this point.
  InformDeletePageDone(callback, (success || offline_ids.size() > 1)
                                     ? DeletePageResult::SUCCESS
                                     : DeletePageResult::STORE_FAILURE);
}

void OfflinePageModel::InformDeletePageDone(const DeletePageCallback& callback,
                                            DeletePageResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.DeletePageResult",
      static_cast<int>(result),
      static_cast<int>(DeletePageResult::RESULT_COUNT));
  archive_manager_->GetStorageStats(
      base::Bind(&ReportStorageHistogramsAfterDelete));
  if (!callback.is_null())
    callback.Run(result);
}

void OfflinePageModel::ScanForMissingArchiveFiles(
    const std::set<base::FilePath>& archive_paths) {
  std::vector<int64_t> ids_of_pages_missing_archive_file;
  std::vector<std::pair<int64_t, ClientId>> offline_client_id_pairs;
  for (const auto& id_page_pair : offline_pages_) {
    if (archive_paths.count(id_page_pair.second.file_path) == 0UL) {
      ids_of_pages_missing_archive_file.push_back(id_page_pair.first);
      offline_client_id_pairs.push_back(
          std::make_pair(id_page_pair.first, id_page_pair.second.client_id));
    }
  }

  // No offline pages missing archive files, we can bail out.
  if (ids_of_pages_missing_archive_file.empty())
    return;

  DeletePageCallback remove_pages_done_callback(
      base::Bind(&OfflinePageModel::OnRemoveOfflinePagesMissingArchiveFileDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_client_id_pairs));

  store_->RemoveOfflinePages(
      ids_of_pages_missing_archive_file,
      base::Bind(&OfflinePageModel::OnRemoveOfflinePagesDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 ids_of_pages_missing_archive_file,
                 remove_pages_done_callback));
}

void OfflinePageModel::OnRemoveOfflinePagesMissingArchiveFileDone(
    const std::vector<std::pair<int64_t, ClientId>>& offline_client_id_pairs,
    DeletePageResult /* result */) {
  for (const auto& id_pair : offline_client_id_pairs) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OfflinePageDeleted(id_pair.first, id_pair.second));
  }
}

void OfflinePageModel::OnRemoveAllFilesDoneForClearAll(
    const base::Closure& callback,
    DeletePageResult result) {
  store_->Reset(base::Bind(&OfflinePageModel::OnResetStoreDoneForClearAll,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback));
}

void OfflinePageModel::OnResetStoreDoneForClearAll(
    const base::Closure& callback, bool success) {
  DCHECK(success);
  if (!success) {
    UMA_HISTOGRAM_ENUMERATION("OfflinePages.ClearAllStatus2",
                              STORE_RESET_FAILED,
                              CLEAR_ALL_STATUS_COUNT);
  }

  offline_pages_.clear();
  store_->Load(base::Bind(&OfflinePageModel::OnReloadStoreDoneForClearAll,
                          weak_ptr_factory_.GetWeakPtr(),
                          callback));
}

void OfflinePageModel::OnReloadStoreDoneForClearAll(
    const base::Closure& callback,
    OfflinePageMetadataStore::LoadStatus load_status,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK_EQ(OfflinePageMetadataStore::LOAD_SUCCEEDED, load_status);
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.ClearAllStatus2",
      load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED ?
          CLEAR_ALL_SUCCEEDED : STORE_RELOAD_FAILED,
      CLEAR_ALL_STATUS_COUNT);

  CacheLoadedData(offline_pages);
  callback.Run();
}

void OfflinePageModel::CacheLoadedData(
    const std::vector<OfflinePageItem>& offline_pages) {
  offline_pages_.clear();
  for (const auto& offline_page : offline_pages)
    offline_pages_[offline_page.offline_id] = offline_page;
}

int64_t OfflinePageModel::GenerateOfflineId() {
  return base::RandGenerator(std::numeric_limits<int64_t>::max()) + 1;
}

void OfflinePageModel::RunWhenLoaded(const base::Closure& task) {
  if (!is_loaded_) {
    delayed_tasks_.push_back(task);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
}

}  // namespace offline_pages
