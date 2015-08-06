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

}  // namespace

OfflinePageModel::OfflinePageModel(
    scoped_ptr<OfflinePageMetadataStore> store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(store.Pass()),
      task_runner_(task_runner),
      weak_ptr_factory_(this) {
}

OfflinePageModel::~OfflinePageModel() {
}

void OfflinePageModel::Shutdown() {
}

void OfflinePageModel::SavePage(const GURL& url,
                                int64 bookmark_id,
                                scoped_ptr<OfflinePageArchiver> archiver,
                                const SavePageCallback& callback) {
  DCHECK(archiver.get());
  archiver->CreateArchive(base::Bind(&OfflinePageModel::OnCreateArchiveDone,
                                     weak_ptr_factory_.GetWeakPtr(), url,
                                     bookmark_id, callback));
  pending_archivers_.push_back(archiver.Pass());
}

void OfflinePageModel::DeletePage(const GURL& url,
                                  const DeletePageCallback& callback) {
  // First we have to load all entries in order to find out the file path
  // for the page to be deleted.
  store_->Load(base::Bind(&OfflinePageModel::OnLoadDoneForDeletion,
                          weak_ptr_factory_.GetWeakPtr(), url, callback));
}

void OfflinePageModel::LoadAllPages(const LoadAllPagesCallback& callback) {
  store_->Load(base::Bind(&OfflinePageModel::OnLoadDone,
                          weak_ptr_factory_.GetWeakPtr(), callback));
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
                 weak_ptr_factory_.GetWeakPtr(), archiver, callback));
}

void OfflinePageModel::OnAddOfflinePageDone(OfflinePageArchiver* archiver,
                                            const SavePageCallback& callback,
                                            bool success) {
  SavePageResult result =
      success ? SavePageResult::SUCCESS : SavePageResult::STORE_FAILURE;
  InformSavePageDone(callback, result);
  DeletePendingArchiver(archiver);
}

void OfflinePageModel::OnLoadDone(
    const LoadAllPagesCallback& callback,
    bool success,
    const std::vector<OfflinePageItem>& offline_pages) {
  // TODO(fgorski): Cache the values here, if we are comfortable with that
  // model. This will require extra handling of parallel loads.
  LoadResult result =
      success ? LoadResult::SUCCESS : LoadResult::STORE_FAILURE;
  callback.Run(result, offline_pages);
}

void OfflinePageModel::InformSavePageDone(const SavePageCallback& callback,
                                          SavePageResult result) {
  callback.Run(result);
}

void OfflinePageModel::DeletePendingArchiver(OfflinePageArchiver* archiver) {
  pending_archivers_.erase(std::find(
      pending_archivers_.begin(), pending_archivers_.end(), archiver));
}

void OfflinePageModel::OnLoadDoneForDeletion(
    const GURL& url,
    const DeletePageCallback& callback,
    bool success,
    const std::vector<OfflinePageItem>& offline_pages) {
  if (!success) {
    callback.Run(DeletePageResult::STORE_FAILURE);
    return;
  }

  for (const auto& page : offline_pages) {
    if (page.url == url) {
      bool* success = new bool(false);
      task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&OfflinePageModel::DeleteArchiverFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     page.file_path,
                     success),
          base::Bind(&OfflinePageModel::OnDeleteArchiverFileDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     url,
                     callback,
                     base::Owned(success)));
      return;
    }
  }

  callback.Run(DeletePageResult::NOT_FOUND);
}

void OfflinePageModel::DeleteArchiverFile(const base::FilePath& file_path,
                                          bool* success) {
  DCHECK(success);
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  *success = base::DeleteFile(file_path, false);
}

void OfflinePageModel::OnDeleteArchiverFileDone(
    const GURL& url,
    const DeletePageCallback& callback,
    const bool* success) {
  DCHECK(success);

  if (!*success) {
    callback.Run(DeletePageResult::DEVICE_FAILURE);
    return;
  }

  store_->RemoveOfflinePage(
      url,
      base::Bind(&OfflinePageModel::OnRemoveOfflinePageDone,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageModel::OnRemoveOfflinePageDone(
    const DeletePageCallback& callback, bool success) {
  callback.Run(
      success ? DeletePageResult::SUCCESS : DeletePageResult::STORE_FAILURE);
}

}  // namespace offline_pages
