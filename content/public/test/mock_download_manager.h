// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_

#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// To avoid leaking download_request_handle.h to embedders.
void PrintTo(const DownloadRequestHandle& params, std::ostream* os);

namespace content {

class MockDownloadManager : public DownloadManager {
 public:
  MockDownloadManager();

  // DownloadManager:
  MOCK_METHOD1(SetDelegate, void(DownloadManagerDelegate* delegate));
  MOCK_CONST_METHOD0(GetDelegate, DownloadManagerDelegate*());
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(GetAllDownloads, void(DownloadVector* downloads));
  MOCK_METHOD2(SearchDownloads, void(const string16& query,
                                     DownloadVector* result));
  MOCK_METHOD1(Init, bool(BrowserContext* browser_context));

  // Gasket for handling scoped_ptr arguments.
  virtual DownloadId StartDownload(
      scoped_ptr<DownloadCreateInfo> info,
      scoped_ptr<ByteStreamReader> stream) OVERRIDE;

  MOCK_METHOD2(MockStartDownload,
               DownloadId(DownloadCreateInfo*,
                          ByteStreamReader*));
  MOCK_METHOD4(UpdateDownload, void(int32 download_id,
                                    int64 bytes_so_far,
                                    int64 bytes_per_sec,
                                    const std::string& hash_state));
  MOCK_METHOD3(OnResponseCompleted, void(int32 download_id,
                                         int64 size,
                                         const std::string& hash));
  MOCK_METHOD1(CancelDownload, void(int32 download_id));
  MOCK_METHOD2(OnDownloadInterrupted,
               void(int32 download_id,
                    DownloadInterruptReason reason));
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
  MOCK_CONST_METHOD0(GetBrowserContext, BrowserContext*());
  MOCK_METHOD0(CheckForHistoryFilesRemoval, void());
  MOCK_METHOD1(GetDownloadItem, DownloadItem*(int id));
  MOCK_METHOD1(GetDownload, DownloadItem*(int id));
  MOCK_METHOD1(SavePageDownloadFinished, void(DownloadItem* download));
  MOCK_METHOD1(GetActiveDownloadItem, DownloadItem*(int id));
  MOCK_METHOD1(GetActiveDownload, DownloadItem*(int32 download_id));

 protected:
  virtual ~MockDownloadManager();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
