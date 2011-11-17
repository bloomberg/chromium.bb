// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
#pragma once

#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_id_factory.h"

class DownloadStatusUpdater;
class DownloadItem;

class MockDownloadManager : public DownloadManager {
 public:
  explicit MockDownloadManager(content::DownloadManagerDelegate* delegate,
                               DownloadIdFactory* id_factory,
                               DownloadStatusUpdater* updater);
  virtual ~MockDownloadManager();

  // DownloadManager:
  virtual void Shutdown() OVERRIDE;
  virtual void GetTemporaryDownloads(const FilePath& dir_path,
                                     DownloadVector* result) OVERRIDE;
  virtual void GetAllDownloads(const FilePath& dir_path,
                               DownloadVector* result) OVERRIDE;
  virtual void SearchDownloads(const string16& query,
                               DownloadVector* result) OVERRIDE;
  virtual bool Init(content::BrowserContext* browser_context) OVERRIDE;
  virtual void StartDownload(int32 id) OVERRIDE;
  virtual void UpdateDownload(int32 download_id, int64 size) OVERRIDE;
  virtual void OnResponseCompleted(int32 download_id, int64 size,
                                   const std::string& hash) OVERRIDE;
  virtual void CancelDownload(int32 download_id) OVERRIDE;
  virtual void OnDownloadInterrupted(int32 download_id, int64 size,
                                     InterruptReason reason) OVERRIDE;
  virtual void DownloadCancelledInternal(DownloadItem* download) OVERRIDE;
  virtual void RemoveDownload(int64 download_handle) OVERRIDE;
  virtual bool IsDownloadReadyForCompletion(DownloadItem* download) OVERRIDE;
  virtual void MaybeCompleteDownload(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadRenamedToFinalName(int download_id,
                                            const FilePath& full_path,
                                            int uniquifier) OVERRIDE;
  virtual int RemoveDownloadsBetween(const base::Time remove_begin,
                                     const base::Time remove_end) OVERRIDE;
  virtual int RemoveDownloads(const base::Time remove_begin) OVERRIDE;
  virtual int RemoveAllDownloads() OVERRIDE;
  virtual void DownloadCompleted(int32 download_id) OVERRIDE;
  virtual void DownloadUrl(const GURL& url,
                           const GURL& referrer,
                           const std::string& referrer_encoding,
                           TabContents* tab_contents) OVERRIDE;
  virtual void DownloadUrlToFile(const GURL& url,
                                 const GURL& referrer,
                                 const std::string& referrer_encoding,
                                 const DownloadSaveInfo& save_info,
                                 TabContents* tab_contents) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void OnPersistentStoreQueryComplete(
      std::vector<DownloadPersistentStoreInfo>* entries) OVERRIDE;
  virtual void OnItemAddedToPersistentStore(int32 download_id,
                                            int64 db_handle) OVERRIDE;
  virtual void ShowDownloadInBrowser(DownloadItem* download) OVERRIDE;
  virtual int InProgressCount() const OVERRIDE;
  virtual content::BrowserContext* BrowserContext() OVERRIDE;
  virtual FilePath LastDownloadPath() OVERRIDE;
  virtual void CreateDownloadItem(
      DownloadCreateInfo* info,
      const DownloadRequestHandle& request_handle) OVERRIDE;
  virtual void ClearLastDownloadPath() OVERRIDE;
  virtual void FileSelected(const FilePath& path, void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;
  virtual void RestartDownload(int32 download_id) OVERRIDE;
  virtual void MarkDownloadOpened(DownloadItem* download) OVERRIDE;
  virtual void CheckForHistoryFilesRemoval() OVERRIDE;
  virtual void CheckForFileRemoval(DownloadItem* download_item) OVERRIDE;
  virtual void AssertQueueStateConsistent(DownloadItem* download) OVERRIDE;
  virtual DownloadItem* GetDownloadItem(int id) OVERRIDE;
  virtual void SavePageDownloadStarted(DownloadItem* download) OVERRIDE;
  virtual void SavePageDownloadFinished(DownloadItem* download) OVERRIDE;
  virtual DownloadItem* GetActiveDownloadItem(int id) OVERRIDE;
  virtual content::DownloadManagerDelegate* delegate() const OVERRIDE;
  virtual void SetDownloadManagerDelegate(
      content::DownloadManagerDelegate* delegate) OVERRIDE;
  virtual DownloadId GetNextId() OVERRIDE;
  virtual void ContinueDownloadWithPath(DownloadItem* download,
                                        const FilePath& chosen_file) OVERRIDE;
  virtual DownloadItem* GetActiveDownload(int32 download_id) OVERRIDE;
  virtual void SetFileManager(DownloadFileManager* file_manager) OVERRIDE;

 private:
  content::DownloadManagerDelegate* delegate_;
  DownloadIdFactory* id_factory_;
  DownloadStatusUpdater* updater_;
  DownloadFileManager* file_manager_;
  std::map<int32, DownloadItem*> item_map_;
  std::map<int32, DownloadItem*> inactive_item_map_;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
