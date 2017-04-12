// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "url/gurl.h"

namespace content {

class DownloadItem;

// Simple class to encapsulate the components of a fetch request.
// TODO(peter): This can likely change to have a single owner, and thus become
// an std::unique_ptr<>, when persistent storage has been implemented.
class CONTENT_EXPORT BackgroundFetchRequestInfo
    : public base::RefCountedThreadSafe<BackgroundFetchRequestInfo> {
 public:
  BackgroundFetchRequestInfo(int request_index,
                             const ServiceWorkerFetchRequest& fetch_request);

  // Populates the cached state for the in-progress |download_item|.
  void PopulateDownloadState(DownloadItem* download_item,
                             DownloadInterruptReason download_interrupt_reason);

  // Populates the response portion of this object from the information made
  // available in the |download_item|.
  void PopulateResponseFromDownloadItem(DownloadItem* download_item);

  // Returns the index of this request within a Background Fetch registration.
  int request_index() const { return request_index_; }

  // Returns the Fetch API Request object that details the developer's request.
  const ServiceWorkerFetchRequest& fetch_request() const {
    return fetch_request_;
  }

  // Returns the response code for the download. Available for both successful
  // and failed requests.
  int GetResponseCode() const;

  // Returns the response text for the download. Available for all started
  // items.
  const std::string& GetResponseText() const;

  // Returns the response headers for the download. Available for both
  // successful and failed requests.
  const std::map<std::string, std::string>& GetResponseHeaders() const;

  // Returns the URL chain for the response, including redirects.
  const std::vector<GURL>& GetURLChain() const;

  // Returns the absolute path to the file in which the response is stored.
  const base::FilePath& GetFilePath() const;

  // Returns the size of the file containing the response, in bytes.
  int64_t GetFileSize() const;

  // Returns the time at which the response was completed.
  const base::Time& GetResponseTime() const;

 private:
  friend class base::RefCountedThreadSafe<BackgroundFetchRequestInfo>;

  ~BackgroundFetchRequestInfo();

  // ---- Data associated with the request -------------------------------------

  int request_index_ = kInvalidBackgroundFetchRequestIndex;
  ServiceWorkerFetchRequest fetch_request_;

  // ---- Data associated with the in-progress download ------------------------

  // Indicates whether download progress data has been populated.
  bool download_state_populated_ = false;

  std::string download_guid_;
  DownloadItem::DownloadState download_state_ = DownloadItem::IN_PROGRESS;

  int response_code_ = 0;
  std::string response_text_;
  std::map<std::string, std::string> response_headers_;

  // ---- Data associated with the response ------------------------------------

  // Indicates whether response data has been populated.
  bool response_data_populated_ = false;

  std::vector<GURL> url_chain_;
  base::FilePath file_path_;
  int64_t file_size_ = 0;
  base::Time response_time_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchRequestInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_REQUEST_INFO_H_
