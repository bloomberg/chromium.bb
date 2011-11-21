// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mock_download_manager.h"

#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_item_impl.h"

MockDownloadManager::MockDownloadManager(
    content::DownloadManagerDelegate* delegate,
    DownloadIdFactory* id_factory,
    DownloadStatusUpdater* updater)
        : delegate_(delegate), id_factory_(id_factory), updater_(updater),
          file_manager_(NULL) {
}

MockDownloadManager::~MockDownloadManager() {
  for (std::map<int32, DownloadItem*>::iterator it = item_map_.begin();
       it != item_map_.end();
       ++it) {
    delete it->second;
  }
  for (std::map<int32, DownloadItem*>::iterator it = inactive_item_map_.begin();
       it != inactive_item_map_.end();
       ++it) {
    delete it->second;
  }
}

void MockDownloadManager::Shutdown() {
}

void MockDownloadManager::GetTemporaryDownloads(const FilePath& dir_path,
                                                DownloadVector* result) {
}

void MockDownloadManager::GetAllDownloads(const FilePath& dir_path,
                                          DownloadVector* result) {
}

void MockDownloadManager::SearchDownloads(const string16& query,
                                          DownloadVector* result) {
}

bool MockDownloadManager::Init(content::BrowserContext* browser_context) {
  return true;
}

void MockDownloadManager::StartDownload(int32 id) {
}

void MockDownloadManager::UpdateDownload(int32 download_id, int64 size) {
}

void MockDownloadManager::OnResponseCompleted(int32 download_id, int64 size,
                                              const std::string& hash) {
}

void MockDownloadManager::CancelDownload(int32 download_id) {
}

void MockDownloadManager::OnDownloadInterrupted(int32 download_id, int64 size,
                                                InterruptReason reason) {
}

void MockDownloadManager::DownloadCancelledInternal(DownloadItem* download) {
  download->Cancel(true);
  item_map_.erase(download->GetId());
  inactive_item_map_[download->GetId()] = download;
}

void MockDownloadManager::RemoveDownload(int64 download_handle) {
}

bool MockDownloadManager::IsDownloadReadyForCompletion(DownloadItem* download) {
  return download->AllDataSaved();
}

void MockDownloadManager::MaybeCompleteDownload(DownloadItem* download) {
  if (IsDownloadReadyForCompletion(download))
    download->OnDownloadRenamedToFinalName(download->GetFullPath());
}

void MockDownloadManager::OnDownloadRenamedToFinalName(
    int download_id,
    const FilePath& full_path,
    int uniquifier) {
}

int MockDownloadManager::RemoveDownloadsBetween(const base::Time remove_begin,
                                                const base::Time remove_end) {
  return 0;
}

int MockDownloadManager::RemoveDownloads(const base::Time remove_begin) {
  return 0;
}

int MockDownloadManager::RemoveAllDownloads() {
  return 1;
}

void MockDownloadManager::DownloadCompleted(int32 download_id) {
}

void MockDownloadManager::DownloadUrl(const GURL& url,
                                      const GURL& referrer,
                                      const std::string& referrer_encoding,
                                      TabContents* tab_contents) {
}

void MockDownloadManager::DownloadUrlToFile(
    const GURL& url,
    const GURL& referrer,
    const std::string& referrer_encoding,
    const DownloadSaveInfo& save_info,
    TabContents* tab_contents) {
}

void MockDownloadManager::AddObserver(Observer* observer) {
}

void MockDownloadManager::RemoveObserver(Observer* observer) {
}

void MockDownloadManager::OnPersistentStoreQueryComplete(
    std::vector<DownloadPersistentStoreInfo>* entries) {
}

void MockDownloadManager::OnItemAddedToPersistentStore(int32 download_id,
                                                       int64 db_handle) {
}

void MockDownloadManager::ShowDownloadInBrowser(DownloadItem* download) {
}

int MockDownloadManager::InProgressCount() const {
  return 1;
}

content::BrowserContext* MockDownloadManager::BrowserContext() {
  return NULL;
}

FilePath MockDownloadManager::LastDownloadPath() {
  return FilePath();
}

void MockDownloadManager::CreateDownloadItem(
    DownloadCreateInfo* info,
    const DownloadRequestHandle& request_handle) {
  item_map_.insert(std::make_pair(
      info->download_id.local(), new DownloadItemImpl(
        this, *info, new DownloadRequestHandle(request_handle), false)));
}

void MockDownloadManager::ClearLastDownloadPath() {
}

void MockDownloadManager::FileSelected(const FilePath& path, void* params) {
}

void MockDownloadManager::FileSelectionCanceled(void* params) {
}

void MockDownloadManager::RestartDownload(int32 download_id) {
}

void MockDownloadManager::MarkDownloadOpened(DownloadItem* download) {
  download->SetOpenWhenComplete(true);
}

void MockDownloadManager::CheckForHistoryFilesRemoval() {
}

void MockDownloadManager::CheckForFileRemoval(DownloadItem* download_item) {
}

void MockDownloadManager::AssertQueueStateConsistent(DownloadItem* download) {
}

DownloadItem* MockDownloadManager::GetDownloadItem(int id) {
  std::map<int32, DownloadItem*>::iterator it = item_map_.find(id);
  if (it == item_map_.end())
    return NULL;
  return it->second;
}

void MockDownloadManager::SavePageDownloadStarted(DownloadItem* download) {
}

void MockDownloadManager::SavePageDownloadFinished(DownloadItem* download) {
}

DownloadItem* MockDownloadManager::GetActiveDownloadItem(int id) {
  return GetDownloadItem(id);
}

content::DownloadManagerDelegate* MockDownloadManager::delegate() const {
  return delegate_;
}

void MockDownloadManager::SetDownloadManagerDelegate(
    content::DownloadManagerDelegate* delegate) {
}

DownloadId MockDownloadManager::GetNextId() {
  return DownloadId(this, 1);
}

void MockDownloadManager::ContinueDownloadWithPath(
    DownloadItem* download,
    const FilePath& chosen_file) {
  download->Rename(chosen_file);
}

DownloadItem* MockDownloadManager::GetActiveDownload(int32 download_id) {
  return GetDownloadItem(download_id);
}

void MockDownloadManager::SetFileManager(DownloadFileManager* file_manager) {
  file_manager_ = file_manager;
}
