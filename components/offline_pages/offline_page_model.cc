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
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/offline_pages/offline_page_item.h"
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
    const std::vector<std::pair<int64, base::FilePath>>& id_path_pairs,
    std::vector<int64>* ids_of_pages_missing_archive_file) {
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

OfflinePageModel::OfflinePageModel(
    scoped_ptr<OfflinePageMetadataStore> store,
    const base::FilePath& archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(store.Pass()),
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
                                int64 bookmark_id,
                                scoped_ptr<OfflinePageArchiver> archiver,
                                const SavePageCallback& callback) {
  DCHECK(is_loaded_);

  // Skip saving the page that is not intended to be saved, like local file
  // page.
  if (!CanSavePage(url)) {
    InformSavePageDone(callback, SavePageResult::SKIPPED);
    return;
  }

  DCHECK(archiver.get());
  archiver->CreateArchive(archives_dir_,
                          base::Bind(&OfflinePageModel::OnCreateArchiveDone,
                                     weak_ptr_factory_.GetWeakPtr(), url,
                                     bookmark_id, base::Time::Now(), callback));
  pending_archivers_.push_back(archiver.Pass());
}

void OfflinePageModel::MarkPageAccessed(int64 bookmark_id) {
  DCHECK(is_loaded_);
  auto iter = offline_pages_.find(bookmark_id);
  if (iter == offline_pages_.end())
    return;

  // Make a copy of the cached item and update it. The cached item should only
  // be updated upon the successful store operation.
  OfflinePageItem offline_page_item = iter->second;
  offline_page_item.last_access_time = base::Time::Now();
  offline_page_item.access_count++;
  store_->AddOrUpdateOfflinePage(
      offline_page_item,
      base::Bind(&OfflinePageModel::OnMarkPageAccesseDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_page_item));
}

void OfflinePageModel::MarkPageForDeletion(int64 bookmark_id,
                                           const DeletePageCallback& callback) {
  DCHECK(is_loaded_);
  auto iter = offline_pages_.find(bookmark_id);
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

void OfflinePageModel::DeletePageByBookmarkId(
    int64 bookmark_id,
    const DeletePageCallback& callback) {
  DCHECK(is_loaded_);
  std::vector<int64> bookmark_ids_to_delete;
  bookmark_ids_to_delete.push_back(bookmark_id);
  DeletePagesByBookmarkId(bookmark_ids_to_delete, callback);
}

void OfflinePageModel::DeletePagesByBookmarkId(
    const std::vector<int64>& bookmark_ids,
    const DeletePageCallback& callback) {
  DCHECK(is_loaded_);

  std::vector<base::FilePath> paths_to_delete;
  for (const auto& bookmark_id : bookmark_ids) {
    auto iter = offline_pages_.find(bookmark_id);
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
      FROM_HERE,
      base::Bind(&DeleteArchiveFiles, paths_to_delete, success),
      base::Bind(&OfflinePageModel::OnDeleteArchiveFilesDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 bookmark_ids,
                 callback,
                 base::Owned(success)));
}

void OfflinePageModel::ClearAll(const base::Closure& callback) {
  DCHECK(is_loaded_);

  std::vector<int64> bookmark_ids;
  for (const auto& id_page_pair : offline_pages_)
    bookmark_ids.push_back(id_page_pair.first);
  DeletePagesByBookmarkId(
      bookmark_ids,
      base::Bind(&OfflinePageModel::OnRemoveAllFilesDoneForClearAll,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

bool OfflinePageModel::HasOfflinePages() const {
  DCHECK(is_loaded_);
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

const OfflinePageItem* OfflinePageModel::GetPageByBookmarkId(
    int64 bookmark_id) const {
  const auto iter = offline_pages_.find(bookmark_id);
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

  std::vector<std::pair<int64, base::FilePath>> id_path_pairs;
  for (const auto& id_page_pair : offline_pages_) {
    id_path_pairs.push_back(
        std::make_pair(id_page_pair.first, id_page_pair.second.file_path));
  }

  std::vector<int64>* ids_of_pages_missing_archive_file =
      new std::vector<int64>();
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&FindPagesMissingArchiveFile, id_path_pairs,
                            ids_of_pages_missing_archive_file),
      base::Bind(&OfflinePageModel::OnFindPagesMissingArchiveFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(ids_of_pages_missing_archive_file)));
}

OfflinePageMetadataStore* OfflinePageModel::GetStoreForTesting() {
  return store_.get();
}

void OfflinePageModel::OnCreateArchiveDone(const GURL& requested_url,
                                           int64 bookmark_id,
                                           const base::Time& start_time,
                                           const SavePageCallback& callback,
                                           OfflinePageArchiver* archiver,
                                           ArchiverResult archiver_result,
                                           const GURL& url,
                                           const base::FilePath& file_path,
                                           int64 file_size) {
  if (requested_url != url) {
    DVLOG(1) << "Saved URL does not match requested URL.";
    // TODO(fgorski): We have created an archive for a wrong URL. It should be
    // deleted from here, once archiver has the right functionality.
    InformSavePageDone(callback, SavePageResult::ARCHIVE_CREATION_FAILED);
    DeletePendingArchiver(archiver);
    return;
  }

  if (archiver_result != ArchiverResult::SUCCESSFULLY_CREATED) {
    SavePageResult result = ToSavePageResult(archiver_result);
    InformSavePageDone(callback, result);
    DeletePendingArchiver(archiver);
    return;
  }

  OfflinePageItem offline_page_item(url, bookmark_id, file_path, file_size,
                                    start_time);
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
    offline_pages_[offline_page.bookmark_id] = offline_page;
    result = SavePageResult::SUCCESS;
    UMA_HISTOGRAM_TIMES(
        "OfflinePages.SavePageTime",
        base::Time::Now() - offline_page.creation_time);
    UMA_HISTOGRAM_MEMORY_KB(
        "OfflinePages.PageSize", offline_page.file_size / 1024);
  } else {
    result = SavePageResult::STORE_FAILURE;
  }
  InformSavePageDone(callback, result);
  DeletePendingArchiver(archiver);

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelChanged(this));
}

void OfflinePageModel::OnMarkPageAccesseDone(
    const OfflinePageItem& offline_page_item, bool success) {
  // Update the item in the cache only upon success.
  if (success)
    offline_pages_[offline_page_item.bookmark_id] = offline_page_item;

  // No need to fire OfflinePageModelChanged event since updating access info
  // should not have any impact to the UI.
}

void OfflinePageModel::OnMarkPageForDeletionDone(
    const OfflinePageItem& offline_page_item,
    const DeletePageCallback& callback,
    bool success) {
  // Update the item in the cache only upon success.
  if (success)
    offline_pages_[offline_page_item.bookmark_id] = offline_page_item;

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

  FOR_EACH_OBSERVER(
      Observer, observers_, OfflinePageDeleted(offline_page_item.bookmark_id));
}

void OfflinePageModel::OnUndoOfflinePageDone(
    const OfflinePageItem& offline_page, bool success) {
  if (!success)
    return;
  offline_pages_[offline_page.bookmark_id] = offline_page;

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelChanged(this));
}

void OfflinePageModel::FinalizePageDeletion() {
  std::vector<int64> bookmark_ids_pending_deletion;
  for (const auto& id_page_pair : offline_pages_) {
    if (!id_page_pair.second.IsMarkedForDeletion())
      continue;
    bookmark_ids_pending_deletion.push_back(id_page_pair.second.bookmark_id);
  }
  DeletePagesByBookmarkId(bookmark_ids_pending_deletion, DeletePageCallback());
}

void OfflinePageModel::UndoPageDeletion(int64 bookmark_id) {
  auto iter = offline_pages_.find(bookmark_id);
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
  UndoPageDeletion(node->id());
}

void OfflinePageModel::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  if (!is_loaded_) {
    delayed_tasks_.push_back(
        base::Bind(&OfflinePageModel::MarkPageForDeletion,
                   weak_ptr_factory_.GetWeakPtr(),
                   node->id(),
                   base::Bind(&EmptyDeleteCallback)));
    return;
  }
  MarkPageForDeletion(node->id(), base::Bind(&EmptyDeleteCallback));
}

void OfflinePageModel::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  // BookmarkNodeChanged could be triggered if title or URL gets changed. If
  // the latter, we need to invalidate the offline copy.
  auto iter = offline_pages_.find(node->id());
  if (iter != offline_pages_.end() && iter->second.url != node->url())
    DeletePageByBookmarkId(node->id(), DeletePageCallback());
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
                                          SavePageResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.SavePageResult",
      static_cast<int>(result),
      static_cast<int>(SavePageResult::RESULT_COUNT));
  callback.Run(result);
}

void OfflinePageModel::DeletePendingArchiver(OfflinePageArchiver* archiver) {
  pending_archivers_.erase(std::find(
      pending_archivers_.begin(), pending_archivers_.end(), archiver));
}

void OfflinePageModel::OnDeleteArchiveFilesDone(
    const std::vector<int64>& bookmark_ids,
    const DeletePageCallback& callback,
    const bool* success) {
  DCHECK(success);

  if (!*success) {
    InformDeletePageDone(callback, DeletePageResult::DEVICE_FAILURE);
    return;
  }

  store_->RemoveOfflinePages(
      bookmark_ids,
      base::Bind(&OfflinePageModel::OnRemoveOfflinePagesDone,
                 weak_ptr_factory_.GetWeakPtr(), bookmark_ids, callback));
}

void OfflinePageModel::OnRemoveOfflinePagesDone(
    const std::vector<int64>& bookmark_ids,
    const DeletePageCallback& callback,
    bool success) {
  // Delete the offline page from the in memory cache regardless of success in
  // store.
  base::Time now = base::Time::Now();
  int64 total_size = 0;
  for (int64 bookmark_id : bookmark_ids) {
    auto iter = offline_pages_.find(bookmark_id);
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
                        OfflinePageDeleted(iter->second.bookmark_id));
    }
    offline_pages_.erase(iter);
  }
  if (bookmark_ids.size() > 1) {
    UMA_HISTOGRAM_COUNTS(
        "OfflinePages.BatchDelete.Count", bookmark_ids.size());
    UMA_HISTOGRAM_MEMORY_KB(
        "OfflinePages.BatchDelete.TotalPageSize", total_size / 1024);
  }
  // Deleting multiple pages always succeeds when it gets to this point.
  InformDeletePageDone(
      callback,
      (success || bookmark_ids.size() > 1) ? DeletePageResult::SUCCESS
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
    const std::vector<int64>* ids_of_pages_missing_archive_file) {
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
    const std::vector<int64>& bookmark_ids,
    OfflinePageModel::DeletePageResult /* result */) {
  for (int64 bookmark_id : bookmark_ids) {
    FOR_EACH_OBSERVER(Observer, observers_, OfflinePageDeleted(bookmark_id));
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
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.ClearAllStatus",
                            STORE_RESET_FAILED,
                            CLEAR_ALL_STATUS_COUNT);

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
      "OfflinePages.ClearAllStatus",
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
    offline_pages_[offline_page.bookmark_id] = offline_page;
}

}  // namespace offline_pages
