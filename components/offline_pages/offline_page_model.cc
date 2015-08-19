// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store.h"
#include "url/gurl.h"

using ArchiverResult = offline_pages::OfflinePageArchiver::ArchiverResult;
using SavePageResult = offline_pages::OfflinePageModel::SavePageResult;

namespace offline_pages {

namespace {

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

}  // namespace

OfflinePageModel::OfflinePageModel(
    scoped_ptr<OfflinePageMetadataStore> store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(store.Pass()),
      is_loaded_(false),
      task_runner_(task_runner),
      weak_ptr_factory_(this) {
  store_->Load(base::Bind(&OfflinePageModel::OnLoadDone,
                          weak_ptr_factory_.GetWeakPtr()));
}

OfflinePageModel::~OfflinePageModel() {
}

void OfflinePageModel::Shutdown() {
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
  DCHECK(archiver.get());
  archiver->CreateArchive(base::Bind(&OfflinePageModel::OnCreateArchiveDone,
                                     weak_ptr_factory_.GetWeakPtr(), url,
                                     bookmark_id, callback));
  pending_archivers_.push_back(archiver.Pass());
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
    callback.Run(DeletePageResult::NOT_FOUND);
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

const std::vector<OfflinePageItem> OfflinePageModel::GetAllPages() const {
  DCHECK(is_loaded_);
  std::vector<OfflinePageItem> offline_pages;
  for (const auto& id_page_pair : offline_pages_)
    offline_pages.push_back(id_page_pair.second);
  return offline_pages;
}

bool OfflinePageModel::GetPageByBookmarkId(
    int64 bookmark_id,
    OfflinePageItem* offline_page) const {
  DCHECK(offline_page);

  const auto iter = offline_pages_.find(bookmark_id);
  if (iter != offline_pages_.end()) {
    *offline_page = iter->second;
    return true;
  }

  return false;
}

const OfflinePageItem* OfflinePageModel::GetPageByOfflineURL(
    const GURL& offline_url) const {
  for (auto iter = offline_pages_.begin();
       iter != offline_pages_.end();
       ++iter) {
    if (iter->second.GetOfflineURL() == offline_url)
      return &(iter->second);
  }
  return nullptr;
}

OfflinePageMetadataStore* OfflinePageModel::GetStoreForTesting() {
  return store_.get();
}

void OfflinePageModel::OnCreateArchiveDone(const GURL& requested_url,
                                           int64 bookmark_id,
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
                                    base::Time::Now());
  store_->AddOfflinePage(
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
  } else {
    result = SavePageResult::STORE_FAILURE;
  }
  InformSavePageDone(callback, result);
  DeletePendingArchiver(archiver);
}

void OfflinePageModel::OnLoadDone(
    bool success,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK(!is_loaded_);
  is_loaded_ = true;

  // TODO(fgorski): Report the UMA upon failure. Cache should probably start
  // empty. See if we can do something about it.
  if (success) {
    for (const auto& offline_page : offline_pages)
      offline_pages_[offline_page.bookmark_id] = offline_page;
  }

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelLoaded(this));
}

void OfflinePageModel::InformSavePageDone(const SavePageCallback& callback,
                                          SavePageResult result) {
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
    callback.Run(DeletePageResult::DEVICE_FAILURE);
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
  for (int64 bookmark_id : bookmark_ids)
    offline_pages_.erase(bookmark_id);
  // Deleting multiple pages always succeeds when it gets to this point.
  if (success || bookmark_ids.size() > 1)
    callback.Run(DeletePageResult::SUCCESS);
  else
    callback.Run(DeletePageResult::STORE_FAILURE);
}

}  // namespace offline_pages
