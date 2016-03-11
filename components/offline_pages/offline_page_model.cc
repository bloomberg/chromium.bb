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
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/proto/offline_pages.pb.h"
#include "url/gurl.h"

using ArchiverResult = offline_pages::OfflinePageArchiver::ArchiverResult;
using SavePageResult = offline_pages::OfflinePageModel::SavePageResult;

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

// Threshold for how old offline copy of a page should be before we offer to
// delete it to free up space.
const base::TimeDelta kPageCleanUpThreshold = base::TimeDelta::FromDays(30);

// The delay for the final deletion to kick in after the page is marked for
// deletion. The value set here is a bit longer that the duration of the
// snackbar that offers undo.
const base::TimeDelta kFinalDeletionDelay = base::TimeDelta::FromSeconds(6);

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
    default:
      NOTREACHED();
      result = SavePageResult::CONTENT_UNAVAILABLE;
  }
  return result;
}

void DeleteArchiveFiles(const std::vector<base::FilePath>& paths_to_delete,
                        bool* success) {
  DCHECK(success);
  for (const auto& file_path : paths_to_delete) {
    // Make sure delete happens on the left of || so that it is always executed.
    *success = base::DeleteFile(file_path, false) || *success;
  }
}

void EmptyDeleteCallback(OfflinePageModel::DeletePageResult /* result */) {
}

void FindPagesMissingArchiveFile(
    const std::vector<std::pair<int64_t, base::FilePath>>& id_path_pairs,
    std::vector<int64_t>* ids_of_pages_missing_archive_file) {
  DCHECK(ids_of_pages_missing_archive_file);

  for (const auto& id_path : id_path_pairs) {
    if (!base::PathExists(id_path.second) ||
        base::DirectoryExists(id_path.second)) {
      ids_of_pages_missing_archive_file->push_back(id_path.first);
    }
  }
}

void EnsureArchivesDirCreated(const base::FilePath& archives_dir) {
  CHECK(base::CreateDirectory(archives_dir));
}

}  // namespace

// static
bool OfflinePageModel::CanSavePage(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

// static
base::TimeDelta OfflinePageModel::GetFinalDeletionDelayForTesting() {
  return kFinalDeletionDelay;
}

OfflinePageModel::OfflinePageModel(
    scoped_ptr<OfflinePageMetadataStore> store,
    const base::FilePath& archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(std::move(store)),
      archives_dir_(archives_dir),
      is_loaded_(false),
      task_runner_(task_runner),
      scoped_observer_(this),
      weak_ptr_factory_(this) {
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(EnsureArchivesDirCreated, archives_dir_),
      base::Bind(&OfflinePageModel::OnEnsureArchivesDirCreatedDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

OfflinePageModel::~OfflinePageModel() {
}

void OfflinePageModel::Start(bookmarks::BookmarkModel* model) {
  scoped_observer_.Add(model);
}

void OfflinePageModel::Shutdown() {
  scoped_observer_.RemoveAll();
}

void OfflinePageModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OfflinePageModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OfflinePageModel::SavePage(const GURL& url,
                                const ClientId& client_id,
                                scoped_ptr<OfflinePageArchiver> archiver,
                                const SavePageCallback& callback) {
  DCHECK(is_loaded_);

  // Skip saving the page that is not intended to be saved, like local file
  // page.
  if (!CanSavePage(url)) {
    InformSavePageDone(callback, SavePageResult::SKIPPED, INVALID_OFFLINE_ID);
    return;
  }

  DCHECK(archiver.get());

  int64_t offline_id = GenerateOfflineId();

  archiver->CreateArchive(
      archives_dir_, base::Bind(&OfflinePageModel::OnCreateArchiveDone,
                                weak_ptr_factory_.GetWeakPtr(), url, offline_id,
                                client_id, base::Time::Now(), callback));
  pending_archivers_.push_back(std::move(archiver));
}

void OfflinePageModel::MarkPageAccessed(int64_t offline_id) {
  DCHECK(is_loaded_);
  auto iter = offline_pages_.find(offline_id);
  if (iter == offline_pages_.end())
    return;

  // MarkPageAccessed should not be called for a page that is being marked for
  // deletion.
  DCHECK(!iter->second.IsMarkedForDeletion());

  // Make a copy of the cached item and update it. The cached item should only
  // be updated upon the successful store operation.
  OfflinePageItem offline_page_item = iter->second;

  base::Time now = base::Time::Now();
  base::TimeDelta time_since_last_accessed =
      now - offline_page_item.last_access_time;

  // The last access time was set to same as creation time when the page was
  // created.
  if (offline_page_item.creation_time == offline_page_item.last_access_time) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("OfflinePages.FirstOpenSinceCreated",
                                time_since_last_accessed.InMinutes(), 1,
                                kMaxOpenedPageHistogramBucket.InMinutes(), 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("OfflinePages.OpenSinceLastOpen",
                                time_since_last_accessed.InMinutes(), 1,
                                kMaxOpenedPageHistogramBucket.InMinutes(), 50);
  }

  offline_page_item.last_access_time = now;
  offline_page_item.access_count++;

  store_->AddOrUpdateOfflinePage(
      offline_page_item,
      base::Bind(&OfflinePageModel::OnMarkPageAccesseDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_page_item));
}

void OfflinePageModel::MarkPageForDeletion(int64_t offline_id,
                                           const DeletePageCallback& callback) {
  DCHECK(is_loaded_);
  auto iter = offline_pages_.find(offline_id);
  if (iter == offline_pages_.end()) {
    InformDeletePageDone(callback, DeletePageResult::NOT_FOUND);
    return;
  }

  // Make a copy of the cached item and update it. The cached item should only
  // be updated upon the successful store operation.
  OfflinePageItem offline_page_item = iter->second;
  offline_page_item.MarkForDeletion();
  store_->AddOrUpdateOfflinePage(
      offline_page_item,
      base::Bind(&OfflinePageModel::OnMarkPageForDeletionDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_page_item, callback));
}

void OfflinePageModel::DeletePageByOfflineId(
    int64_t offline_id,
    const DeletePageCallback& callback) {
  DCHECK(is_loaded_);
  std::vector<int64_t> offline_ids_to_delete;
  offline_ids_to_delete.push_back(offline_id);
  DeletePagesByOfflineId(offline_ids_to_delete, callback);
}

void OfflinePageModel::DeletePagesByOfflineId(
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

  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&DeleteArchiveFiles, paths_to_delete, success),
      base::Bind(&OfflinePageModel::OnDeleteArchiveFilesDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_ids, callback,
                 base::Owned(success)));
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

bool OfflinePageModel::HasOfflinePages() const {
  // Since offline pages feature is enabled by default,
  // NetErrorTabHelper::SetHasOfflinePages might call this before the model is
  // fully loaded. To address this, we need to switch to asynchonous model
  // (crbug.com/589526). But for now, we just bail out to work around the test
  // issue.
  if (!is_loaded_)
    return false;

  // Check that at least one page is not marked for deletion. Because we have
  // pages marked for deletion, we cannot simply invert result of |empty()|.
  for (const auto& id_page_pair : offline_pages_) {
    if (!id_page_pair.second.IsMarkedForDeletion())
      return true;
  }
  return false;
}

const std::vector<OfflinePageItem> OfflinePageModel::GetAllPages() const {
  DCHECK(is_loaded_);
  std::vector<OfflinePageItem> offline_pages;
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.IsMarkedForDeletion())
      continue;
    offline_pages.push_back(id_page_pair.second);
  }
  return offline_pages;
}

const std::vector<OfflinePageItem> OfflinePageModel::GetPagesToCleanUp() const {
  DCHECK(is_loaded_);
  std::vector<OfflinePageItem> offline_pages;
  base::Time now = base::Time::Now();
  for (const auto& id_page_pair : offline_pages_) {
    if (!id_page_pair.second.IsMarkedForDeletion() &&
        now - id_page_pair.second.last_access_time > kPageCleanUpThreshold) {
      offline_pages.push_back(id_page_pair.second);
    }
  }
  return offline_pages;
}

const std::vector<int64_t> OfflinePageModel::GetOfflineIdsForClientId(
    const ClientId& cid) const {
  std::vector<int64_t> results;

  // TODO(bburns): actually use an index rather than linear scan.
  const std::vector<OfflinePageItem> offline_pages = GetAllPages();

  for (size_t i = 0; i < offline_pages.size(); i++) {
    if (offline_pages[i].client_id.name_space == cid.name_space &&
        offline_pages[i].client_id.id == cid.id) {
      results.push_back(offline_pages[i].offline_id);
    }
  }
  return results;
}

const OfflinePageItem* OfflinePageModel::GetPageByOfflineId(
    int64_t offline_id) const {
  const auto iter = offline_pages_.find(offline_id);
  return iter != offline_pages_.end() && !iter->second.IsMarkedForDeletion()
             ? &(iter->second)
             : nullptr;
}

const OfflinePageItem* OfflinePageModel::GetPageByOfflineURL(
    const GURL& offline_url) const {
  for (auto iter = offline_pages_.begin();
       iter != offline_pages_.end();
       ++iter) {
    if (iter->second.GetOfflineURL() == offline_url &&
        !iter->second.IsMarkedForDeletion()) {
      return &(iter->second);
    }
  }
  return nullptr;
}

const OfflinePageItem* OfflinePageModel::GetPageByOnlineURL(
    const GURL& online_url) const {
  for (auto iter = offline_pages_.begin(); iter != offline_pages_.end();
       ++iter) {
    if (iter->second.url == online_url && !iter->second.IsMarkedForDeletion())
      return &(iter->second);
  }
  return nullptr;
}

void OfflinePageModel::CheckForExternalFileDeletion() {
  DCHECK(is_loaded_);

  std::vector<std::pair<int64_t, base::FilePath>> id_path_pairs;
  for (const auto& id_page_pair : offline_pages_) {
    id_path_pairs.push_back(
        std::make_pair(id_page_pair.first, id_page_pair.second.file_path));
  }

  std::vector<int64_t>* ids_of_pages_missing_archive_file =
      new std::vector<int64_t>();
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&FindPagesMissingArchiveFile, id_path_pairs,
                            ids_of_pages_missing_archive_file),
      base::Bind(&OfflinePageModel::OnFindPagesMissingArchiveFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(ids_of_pages_missing_archive_file)));
}

void OfflinePageModel::RecordStorageHistograms(int64_t total_space_bytes,
                                               int64_t free_space_bytes,
                                               bool reporting_after_delete) {
  // Total space taken by offline pages.
  int64_t total_page_size = 0;
  for (const auto& id_page_pair : offline_pages_) {
    if (!id_page_pair.second.IsMarkedForDeletion())
      continue;
    total_page_size += id_page_pair.second.file_size;
  }

  int total_page_size_mb = static_cast<int>(total_page_size / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("OfflinePages.TotalPageSize", total_page_size_mb);

  // How much of the total space the offline pages take.
  int total_page_size_percentage =
      static_cast<int>(1.0 * total_page_size / total_space_bytes * 100);
  UMA_HISTOGRAM_PERCENTAGE("OfflinePages.TotalPageSizePercentage",
                           total_page_size_percentage);

  // If the user is deleting the pages, perhaps they are running out of free
  // space. Report the size before the operation, where a base for calculation
  // of total free space includes space taken by offline pages.
  if (reporting_after_delete && free_space_bytes > 0) {
    int percentage_of_free = static_cast<int>(
        1.0 * total_page_size / (total_page_size + free_space_bytes) * 100);
    UMA_HISTOGRAM_PERCENTAGE(
        "OfflinePages.DeletePage.TotalPageSizeAsPercentageOfFreeSpace",
        percentage_of_free);
  }
}

OfflinePageMetadataStore* OfflinePageModel::GetStoreForTesting() {
  return store_.get();
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
                       offline_id);
    DeletePendingArchiver(archiver);
    return;
  }

  if (archiver_result != ArchiverResult::SUCCESSFULLY_CREATED) {
    SavePageResult result = ToSavePageResult(archiver_result);
    InformSavePageDone(callback, result, offline_id);
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
        "OfflinePages.SavePageTime",
        base::Time::Now() - offline_page.creation_time);
    UMA_HISTOGRAM_MEMORY_KB(
        "OfflinePages.PageSize", offline_page.file_size / 1024);
  } else {
    result = SavePageResult::STORE_FAILURE;
  }
  InformSavePageDone(callback, result, offline_page.offline_id);
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

void OfflinePageModel::OnMarkPageForDeletionDone(
    const OfflinePageItem& offline_page_item,
    const DeletePageCallback& callback,
    bool success) {
  // Update the item in the cache only upon success.
  if (success)
    offline_pages_[offline_page_item.offline_id] = offline_page_item;

  InformDeletePageDone(callback, success ? DeletePageResult::SUCCESS
                                         : DeletePageResult::STORE_FAILURE);

  if (!success)
    return;

  // Schedule to do the final deletion.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&OfflinePageModel::FinalizePageDeletion,
                 weak_ptr_factory_.GetWeakPtr()),
      kFinalDeletionDelay);

  FOR_EACH_OBSERVER(Observer, observers_,
                    OfflinePageDeleted(offline_page_item.offline_id));
}

void OfflinePageModel::OnUndoOfflinePageDone(
    const OfflinePageItem& offline_page, bool success) {
  if (!success)
    return;
  offline_pages_[offline_page.offline_id] = offline_page;

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelChanged(this));
}

void OfflinePageModel::FinalizePageDeletion() {
  std::vector<int64_t> offline_ids_pending_deletion;
  for (const auto& id_page_pair : offline_pages_) {
    if (!id_page_pair.second.IsMarkedForDeletion())
      continue;
    offline_ids_pending_deletion.push_back(id_page_pair.second.offline_id);
  }
  DeletePagesByOfflineId(offline_ids_pending_deletion, DeletePageCallback());
}

void OfflinePageModel::UndoPageDeletion(int64_t offline_id) {
  auto iter = offline_pages_.find(offline_id);
  if (iter == offline_pages_.end())
    return;

  // Make a copy of the cached item and update it. The cached item should only
  // be updated upon the successful store operation.
  OfflinePageItem offline_page_item = iter->second;
  if (!offline_page_item.IsMarkedForDeletion())
    return;

  // Clear the flag to bring it back.
  offline_page_item.ClearMarkForDeletion();
  store_->AddOrUpdateOfflinePage(
      offline_page_item,
      base::Bind(&OfflinePageModel::OnUndoOfflinePageDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_page_item));
}

void OfflinePageModel::BookmarkModelChanged() {
}

void OfflinePageModel::BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                                         const bookmarks::BookmarkNode* parent,
                                         int index) {
  const bookmarks::BookmarkNode* node = parent->GetChild(index);
  DCHECK(node);
  ClientId client_id(BOOKMARK_NAMESPACE, base::Int64ToString(node->id()));
  std::vector<int64_t> ids;
  // In this case we want only all pages, including those marked for deletion.
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.client_id == client_id) {
      ids.push_back(id_page_pair.second.offline_id);
    }
  }

  for (size_t i = 0; i < ids.size(); i++) {
    UndoPageDeletion(ids[i]);
  }
}

void OfflinePageModel::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  ClientId cid;
  cid.name_space = BOOKMARK_NAMESPACE;
  cid.id = base::Int64ToString(node->id());
  std::vector<int64_t> ids = GetOfflineIdsForClientId(cid);
  if (!is_loaded_) {
    for (size_t i = 0; i < ids.size(); i++) {
      delayed_tasks_.push_back(
          base::Bind(&OfflinePageModel::MarkPageForDeletion,
                     weak_ptr_factory_.GetWeakPtr(), ids[i],
                     base::Bind(&EmptyDeleteCallback)));
    }
    return;
  }
  for (size_t i = 0; i < ids.size(); i++) {
    MarkPageForDeletion(ids[i], base::Bind(&EmptyDeleteCallback));
  }
}

void OfflinePageModel::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  // BookmarkNodeChanged could be triggered if title or URL gets changed. If
  // the latter, we need to invalidate the offline copy.
  ClientId cid;
  cid.name_space = BOOKMARK_NAMESPACE;
  cid.id = base::Int64ToString(node->id());
  std::vector<int64_t> ids = GetOfflineIdsForClientId(cid);
  for (size_t i = 0; i < ids.size(); i++) {
    auto iter = offline_pages_.find(ids[i]);
    if (iter != offline_pages_.end() && iter->second.url != node->url())
      DeletePageByOfflineId(ids[i], DeletePageCallback());
  }
}

void OfflinePageModel::OnEnsureArchivesDirCreatedDone() {
  store_->Load(base::Bind(&OfflinePageModel::OnLoadDone,
                          weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageModel::OnLoadDone(
    OfflinePageMetadataStore::LoadStatus load_status,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK(!is_loaded_);
  is_loaded_ = true;

  // TODO(jianli): rebuild the store upon failure.

  if (load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED)
    CacheLoadedData(offline_pages);

  // Run all the delayed tasks.
  for (const auto& delayed_task : delayed_tasks_)
    delayed_task.Run();
  delayed_tasks_.clear();

  // If there are pages that are marked for deletion, but not yet deleted and
  // OfflinePageModel gets reloaded. Delete the pages now.
  FinalizePageDeletion();

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelLoaded(this));

  CheckForExternalFileDeletion();
}

void OfflinePageModel::InformSavePageDone(const SavePageCallback& callback,
                                          SavePageResult result,
                                          int64_t offline_id) {
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.SavePageResult",
      static_cast<int>(result),
      static_cast<int>(SavePageResult::RESULT_COUNT));
  callback.Run(result, offline_id);
}

void OfflinePageModel::DeletePendingArchiver(OfflinePageArchiver* archiver) {
  pending_archivers_.erase(std::find(
      pending_archivers_.begin(), pending_archivers_.end(), archiver));
}

void OfflinePageModel::OnDeleteArchiveFilesDone(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback,
    const bool* success) {
  DCHECK(success);

  if (!*success) {
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
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.PageLifetime",
        (now - iter->second.creation_time).InMinutes(),
        1,
        base::TimeDelta::FromDays(365).InMinutes(),
        100);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.DeletePage.TimeSinceLastOpen",
        (now - iter->second.last_access_time).InMinutes(),
        1,
        base::TimeDelta::FromDays(365).InMinutes(),
        100);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.DeletePage.LastOpenToCreated",
        (iter->second.last_access_time - iter->second.creation_time).
            InMinutes(),
        1,
        base::TimeDelta::FromDays(365).InMinutes(),
        100);
    UMA_HISTOGRAM_MEMORY_KB(
        "OfflinePages.DeletePage.PageSize", iter->second.file_size / 1024);
    UMA_HISTOGRAM_COUNTS(
        "OfflinePages.DeletePage.AccessCount", iter->second.access_count);
    // If the page is not marked for deletion at this point, the model has not
    // yet informed the observer that the offline page is deleted.
    if (!iter->second.IsMarkedForDeletion()) {
      FOR_EACH_OBSERVER(Observer, observers_,
                        OfflinePageDeleted(iter->second.offline_id));
    }
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
  if (!callback.is_null())
    callback.Run(result);
}

void OfflinePageModel::OnFindPagesMissingArchiveFile(
    const std::vector<int64_t>* ids_of_pages_missing_archive_file) {
  DCHECK(ids_of_pages_missing_archive_file);
  if (ids_of_pages_missing_archive_file->empty())
    return;

  DeletePageCallback done_callback(
      base::Bind(&OfflinePageModel::OnRemoveOfflinePagesMissingArchiveFileDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 *ids_of_pages_missing_archive_file));

  store_->RemoveOfflinePages(
      *ids_of_pages_missing_archive_file,
      base::Bind(&OfflinePageModel::OnRemoveOfflinePagesDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 *ids_of_pages_missing_archive_file,
                 done_callback));
}

void OfflinePageModel::OnRemoveOfflinePagesMissingArchiveFileDone(
    const std::vector<int64_t>& offline_ids,
    OfflinePageModel::DeletePageResult /* result */) {
  for (int64_t offline_id : offline_ids) {
    FOR_EACH_OBSERVER(Observer, observers_, OfflinePageDeleted(offline_id));
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

}  // namespace offline_pages
