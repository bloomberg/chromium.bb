// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
#pragma once

#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockDownloadManager : public content::DownloadManager {
 public:
  MockDownloadManager();
  virtual ~MockDownloadManager();

  // DownloadManager:
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD2(GetTemporaryDownloads, void(const FilePath& dir_path,
                                           DownloadVector* result));
  MOCK_METHOD2(GetAllDownloads, void(const FilePath& dir_path,
                                     DownloadVector* result));
  MOCK_METHOD2(SearchDownloads, void(const string16& query,
                                     DownloadVector* result));
  MOCK_METHOD1(Init, bool(content::BrowserContext* browser_context));
  MOCK_METHOD1(StartDownload, void(int32 id));
  MOCK_METHOD4(UpdateDownload, void(int32 download_id,
                                    int64 bytes_so_far,
                                    int64 bytes_per_sec,
                                    const std::string& hash_state));
  MOCK_METHOD3(OnResponseCompleted, void(int32 download_id,
                                         int64 size,
                                         const std::string& hash));
  MOCK_METHOD1(CancelDownload, void(int32 download_id));
  MOCK_METHOD4(OnDownloadInterrupted,
               void(int32 download_id,
                    int64 size,
                    const std::string& hash_state,
                    content::DownloadInterruptReason reason));
  MOCK_METHOD3(OnDownloadRenamedToFinalName, void(int download_id,
                                                  const FilePath& full_path,
                                                  int uniquifier));
  MOCK_METHOD2(RemoveDownloadsBetween, int(base::Time remove_begin,
                                           base::Time remove_end));
  MOCK_METHOD1(RemoveDownloads, int(base::Time remove_begin));
  MOCK_METHOD0(RemoveAllDownloads, int());
  MOCK_METHOD1(DownloadUrlMock, void(DownloadUrlParameters*));
  virtual void DownloadUrl(scoped_ptr<DownloadUrlParameters> params) OVERRIDE {
    DownloadUrlMock(params.get());
  }
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD1(OnPersistentStoreQueryComplete, void(
      std::vector<DownloadPersistentStoreInfo>* entries));
  MOCK_METHOD2(OnItemAddedToPersistentStore, void(int32 download_id,
                                                  int64 db_handle));
  MOCK_CONST_METHOD0(InProgressCount, int());
  MOCK_CONST_METHOD0(GetBrowserContext, content::BrowserContext*());
  MOCK_METHOD0(LastDownloadPath, FilePath());
  MOCK_METHOD2(CreateDownloadItem, net::BoundNetLog(
      DownloadCreateInfo* info,
      const DownloadRequestHandle& request_handle));
  MOCK_METHOD5(CreateSavePackageDownloadItem, content::DownloadItem*(
      const FilePath& main_file_path,
      const GURL& page_url,
      bool is_otr,
      const std::string& mime_type,
      content::DownloadItem::Observer* observer));
  MOCK_METHOD0(ClearLastDownloadPath, void());
  MOCK_METHOD2(FileSelected, void(const FilePath& path, int32 download_id));
  MOCK_METHOD1(FileSelectionCanceled, void(int32 download_id));
  MOCK_METHOD1(RestartDownload, void(int32 download_id));
  MOCK_METHOD0(CheckForHistoryFilesRemoval, void());
  MOCK_METHOD1(GetDownloadItem, content::DownloadItem*(int id));
  MOCK_METHOD1(SavePageDownloadFinished, void(content::DownloadItem* download));
  MOCK_METHOD1(GetActiveDownloadItem, content::DownloadItem*(int id));
  MOCK_METHOD0(GenerateFileHash, bool());
  MOCK_CONST_METHOD0(delegate, content::DownloadManagerDelegate*());
  MOCK_METHOD1(SetDownloadManagerDelegate, void(
      content::DownloadManagerDelegate* delegate));
  MOCK_METHOD2(ContinueDownloadWithPath, void(content::DownloadItem* download,
                                              const FilePath& chosen_file));
  MOCK_METHOD1(GetActiveDownload, content::DownloadItem*(int32 download_id));
  MOCK_METHOD1(SetFileManager, void(DownloadFileManager* file_manager));
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
