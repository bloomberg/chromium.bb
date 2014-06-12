// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_

#include <string>
#include <vector>

#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class DownloadRequestHandle;

namespace content {

// To avoid leaking download_request_handle.h to embedders.
void PrintTo(const DownloadRequestHandle& params, std::ostream* os);

class MockDownloadManager : public DownloadManager {
 public:
  // Structure to make it possible to match more than 10 arguments on
  // CreateDownloadItem.
  struct CreateDownloadItemAdapter {
    uint32 id;
    base::FilePath current_path;
    base::FilePath target_path;
    std::vector<GURL> url_chain;
    GURL referrer_url;
    std::string mime_type;
    std::string original_mime_type;
    base::Time start_time;
    base::Time end_time;
    std::string etag;
    std::string last_modified;
    int64 received_bytes;
    int64 total_bytes;
    DownloadItem::DownloadState state;
    DownloadDangerType danger_type;
    DownloadInterruptReason interrupt_reason;
    bool opened;

    CreateDownloadItemAdapter(
      uint32 id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      const base::Time& start_time,
      const base::Time& end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64 received_bytes,
      int64 total_bytes,
      DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened);
    // Required by clang compiler.
    CreateDownloadItemAdapter(const CreateDownloadItemAdapter& rhs);
    ~CreateDownloadItemAdapter();

    bool operator==(const CreateDownloadItemAdapter& rhs);
  };

  MockDownloadManager();
  virtual ~MockDownloadManager();

  // DownloadManager:
  MOCK_METHOD1(SetDelegate, void(DownloadManagerDelegate* delegate));
  MOCK_CONST_METHOD0(GetDelegate, DownloadManagerDelegate*());
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(GetAllDownloads, void(DownloadVector* downloads));
  MOCK_METHOD1(Init, bool(BrowserContext* browser_context));

  // Gasket for handling scoped_ptr arguments.
  virtual void StartDownload(
      scoped_ptr<DownloadCreateInfo> info,
      scoped_ptr<ByteStreamReader> stream,
      const DownloadUrlParameters::OnStartedCallback& callback) OVERRIDE;

  MOCK_METHOD2(MockStartDownload,
               void(DownloadCreateInfo*, ByteStreamReader*));
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

  // Redirects to mock method to get around gmock 10 argument limit.
  virtual DownloadItem* CreateDownloadItem(
      uint32 id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      const base::Time& start_time,
      const base::Time& end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64 received_bytes,
      int64 total_bytes,
      DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened) OVERRIDE;

  MOCK_METHOD1(MockCreateDownloadItem,
               DownloadItem*(CreateDownloadItemAdapter adapter));

  MOCK_CONST_METHOD0(InProgressCount, int());
  MOCK_CONST_METHOD0(NonMaliciousInProgressCount, int());
  MOCK_CONST_METHOD0(GetBrowserContext, BrowserContext*());
  MOCK_METHOD0(CheckForHistoryFilesRemoval, void());
  MOCK_METHOD1(GetDownload, DownloadItem*(uint32 id));
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
